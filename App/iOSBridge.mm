/* iOSBridge.mm - iOS 平台 Bridge：MIDlet 运行时 + 平台 natives
 * 帧循环/键事件/生命周期均由主线程驱动（VM 无锁，与 headless 验证模式一致）。
 * http 用 NSURLSession + 信号量同步等待（native 层本就要求同步语义）。
 */
#include "iOSBridge.h"
#include "vm.h"
#include "bridge_common.h"

#include <stdarg.h>

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>

/* VM 层 zip.c 提供 */
extern "C" uint8_t* zipReadFile(const char* jarPath, const char* entryName, size_t* outLen);

/* ============ 全局状态 ============ */
static VMThread* gThread = NULL;
static Object*   gMidlet = NULL;
static int       gRunning = 0;
static char      gLastError[512] = {0};
static char      gMidletName[128] = {0};
static char      gMainClass[256] = {0};

static Class* gDispCls = NULL;
static Class* gCanvasCls = NULL;
static Field* gFCur = NULL;   /* Display.current */
static Field* gFBuf = NULL;   /* Canvas.buffer   */
static Field* gFW = NULL;     /* Canvas.bufW     */
static Field* gFH = NULL;     /* Canvas.bufH     */

static void setErr(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gLastError, sizeof(gLastError), fmt, ap);
    va_end(ap);
    NSLog(@"[j2me] ERROR: %s", gLastError);
}

/* ============ 平台 hooks：日志走 NSLog ============ */
static void logHook(const char* msg) { NSLog(@"[j2me] %s", msg); }
static void midletStateHook(int state) {
    NSLog(@"[j2me] midlet state=%d", state);
}
static PlatformHooks gHooks = { midletStateHook, logHook };

/* ============ iOS 平台 natives ============ */

static Slot n_noop(VMThread* t, Slot* a) { (void)t; (void)a; Slot r; r.ref = NULL; return r; }

static Slot n_vibrate(VMThread* t, Slot* a) {
    (void)t; (void)a;
    AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
    Slot r; r.ref = NULL; r.i = 1;
    return r;
}

static Slot n_midletNotify(VMThread* t, Slot* a) {
    (void)t;
    NSLog(@"[j2me] midletNotify state=%d", (int)a[0].i);
    Slot r; r.ref = NULL;
    return r;
}

/* 读游戏 jar 的 META-INF/MANIFEST.MF 属性（与 headless 相同） */
static Slot n_getAppProperty(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    Object* s = UNREF(a[0].ref);
    if (!s) return r;
    char* key = vmStringToUTF(t, s);
    if (!key) return r;
    size_t len = 0;
    char* mf = (char*)zipReadFile(NULL, "META-INF/MANIFEST.MF", &len);
    if (!mf) mf = (char*)zipReadFile(NULL, "meta-inf/MANIFEST.MF", &len);
    if (mf) {
        char* text = (char*)malloc(len + 1);
        if (text) {
            memcpy(text, mf, len);
            text[len] = 0;
            char* line = text;
            while (line && *line) {
                char* nl = strchr(line, '\n');
                if (nl) *nl = 0;
                char* cr = strchr(line, '\r');
                if (cr) *cr = 0;
                char* colon = strchr(line, ':');
                if (colon) {
                    *colon = 0;
                    char* v = colon + 1;
                    while (*v == ' ') v++;
                    if (strcasecmp(line, key) == 0) {
                        r.ref = JREF(vmNewStringUTF(t, v));
                        break;
                    }
                }
                line = nl ? nl + 1 : NULL;
            }
            free(text);
        }
        free(mf);
    }
    free(key);
    return r;
}

/* http: NSURLSession 同步执行。
 * 参数: (url, method, headers "k\r\nv\r\n", body) -> byte[] 或 null(失败)
 * 返回打包: code(u2) msgLen(u2) msg hdrLen(u4) hdr bodyLen(u4) body（均大端） */
static Slot n_httpExecute(VMThread* t, Slot* a) {
    /* 游戏线程可能无 autorelease pool，整体包裹防泄漏 */
    @autoreleasepool {
    Slot r;
    r.ref = NULL;
    Object* urlS = UNREF(a[0].ref);
    Object* mtdS = UNREF(a[1].ref);
    Object* hdrS = UNREF(a[2].ref);
    ArrayObject* body = (ArrayObject*)UNREF(a[3].ref);
    if (!urlS || !mtdS) return r;

    char* url = vmStringToUTF(t, urlS);
    char* method = vmStringToUTF(t, mtdS);
    char* headers = hdrS ? vmStringToUTF(t, hdrS) : NULL;
    if (!url || !method) { free(url); free(method); free(headers); return r; }

    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]];
    req.HTTPMethod = [NSString stringWithUTF8String:method];
    req.timeoutInterval = 30.0;
    if (headers && *headers) {
        /* "k\r\nv\r\n" 成对解析 */
        NSArray* parts = [[NSString stringWithUTF8String:headers]
            componentsSeparatedByString:@"\r\n"];
        for (NSUInteger i = 0; i + 1 < [parts count]; i += 2) {
            if ([parts[i] length] == 0) continue;
            [req setValue:parts[i + 1] forHTTPHeaderField:parts[i]];
        }
    }
    if (body && body->length > 0) {
        req.HTTPBody = [NSData dataWithBytes:ARRAY_DATA(body) length:body->length];
    }

    __block NSData* respData = nil;
    __block NSHTTPURLResponse* httpResp = nil;
    __block NSError* err = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSessionDataTask* task = [[NSURLSession sharedSession]
        dataTaskWithRequest:req completionHandler:^(NSData* data, NSURLResponse* resp, NSError* e) {
            respData = data;
            httpResp = [resp isKindOfClass:[NSHTTPURLResponse class]] ? (NSHTTPURLResponse*)resp : nil;
            err = e;
            dispatch_semaphore_signal(sem);
        }];
    [task resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    free(url); free(method); free(headers);
    if (err || !httpResp) {
        NSLog(@"[j2me] http 失败: %@", err.localizedDescription);
        return r;
    }

    /* 打包响应 */
    NSString* msgStr = [NSHTTPURLResponse localizedStringForStatusCode:httpResp.statusCode];
    NSData* msgData = [msgStr dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableData* hdrData = [NSMutableData data];
    for (NSString* k in httpResp.allHeaderFields) {
        NSString* v = [httpResp.allHeaderFields objectForKey:k];
        [hdrData appendData:[[NSString stringWithFormat:@"%@: %@\r\n", k, v]
            dataUsingEncoding:NSUTF8StringEncoding]];
    }
    NSUInteger msgLen = [msgData length];
    NSUInteger hdrLen = [hdrData length];
    NSUInteger bodyLen = [respData length];
    NSUInteger total = 2 + 2 + msgLen + 4 + hdrLen + 4 + bodyLen;

    t->gcDisable++;
    ArrayObject* out = vmNewTypeArray(t, EK_BYTE, (int32_t)total);
    t->gcDisable--;
    if (!out) return r;
    uint8_t* p = (uint8_t*)ARRAY_DATA(out);
    int code = (int)httpResp.statusCode;
    *p++ = (uint8_t)(code >> 8); *p++ = (uint8_t)code;
    *p++ = (uint8_t)(msgLen >> 8); *p++ = (uint8_t)msgLen;
    memcpy(p, [msgData bytes], msgLen); p += msgLen;
    uint32_t hl = (uint32_t)hdrLen;
    *p++ = (uint8_t)(hl >> 24); *p++ = (uint8_t)(hl >> 16);
    *p++ = (uint8_t)(hl >> 8);  *p++ = (uint8_t)hl;
    memcpy(p, [hdrData bytes], hdrLen); p += hdrLen;
    uint32_t bl = (uint32_t)bodyLen;
    *p++ = (uint8_t)(bl >> 24); *p++ = (uint8_t)(bl >> 16);
    *p++ = (uint8_t)(bl >> 8);  *p++ = (uint8_t)bl;
    if (bodyLen) memcpy(p, [respData bytes], bodyLen);

    r.ref = JREF((Object*)out);
    return r;
    } /* @autoreleasepool */
}

static void registerPlatformNatives(void) {
    vmRegisterNative("javax/microedition/lcdui/Canvas", "nativeRepaint", "()V", n_noop);
    vmRegisterNative("javax/microedition/lcdui/Screen", "nativeRepaint", "()V", n_noop);
    vmRegisterNative("javax/microedition/lcdui/Display", "nativeDisplayChanged", "()V", n_noop);
    vmRegisterNative("javax/microedition/lcdui/Display", "nativeVibrate", "(I)Z", n_vibrate);
    vmRegisterNative("javax/microedition/midlet/MIDlet", "nativeMidletNotify", "(I)V", n_midletNotify);
    vmRegisterNative("javax/microedition/midlet/MIDlet", "nativeGetAppProperty",
                     "(Ljava/lang/String;)Ljava/lang/String;", n_getAppProperty);
    vmRegisterNative("javax/microedition/io/Connector$HttpConnectionImpl", "nativeHttpExecute",
                     "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[B)[B", n_httpExecute);
}

/* ============ manifest 解析 ============ */

/* 从 manifest 文本取属性值（键不分大小写），写入 out（cap 字节），成功返回 1 */
static int manifestGet(const char* text, const char* key, char* out, size_t cap) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s:", key);
    size_t plen = strlen(pattern);
    const char* line = text;
    while (line && *line) {
        const char* nl = strchr(line, '\n');
        size_t ll = nl ? (size_t)(nl - line) : strlen(line);
        if (ll > plen && strncasecmp(line, pattern, plen) == 0) {
            const char* v = line + plen;
            while (*v == ' ' || *v == '\t') v++;
            size_t vl = ll - (size_t)(v - line);
            if (vl >= cap) vl = cap - 1;
            memcpy(out, v, vl);
            out[vl] = 0;
            /* 去尾部空白 */
            for (char* e = out + vl - 1; e >= out && (*e == ' ' || *e == '\r' || *e == '\t'); e--) *e = 0;
            return 1;
        }
        line = nl ? nl + 1 : NULL;
    }
    return 0;
}

/* MIDlet-1: "Name, Icon, Class" 取第 1/3 段 */
static int parseMidlet1(const char* text, char* name, size_t nameCap,
                        char* cls, size_t clsCap) {
    char m1[512] = {0};
    if (!manifestGet(text, "MIDlet-1", m1, sizeof(m1))) return 0;
    /* 三段逗号分割 */
    const char* p1 = strchr(m1, ',');
    if (!p1) return 0;
    const char* p2 = strchr(p1 + 1, ',');
    if (!p2) return 0;
    /* name = m1..p1 */
    size_t nl = (size_t)(p1 - m1);
    if (nl >= nameCap) nl = nameCap - 1;
    memcpy(name, m1, nl); name[nl] = 0;
    /* class = p2+1.. 修剪空白与 ./ 前缀 */
    const char* cs = p2 + 1;
    while (*cs == ' ') cs++;
    if (cs[0] == '.' && cs[1] == '/') cs += 2;
    size_t cl = strlen(cs);
    while (cl > 0 && (cs[cl-1] == ' ' || cs[cl-1] == '\r')) cl--;
    if (cl == 0 || cl >= clsCap) return 0;
    memcpy(cls, cs, cl); cls[cl] = 0;
    /* 点分名 -> 斜杠名 */
    for (char* q = cls; *q; q++) if (*q == '.') *q = '/';
    return 1;
}

/* ============ 异常打印 ============ */

static void printException(VMThread* t, const char* where) {
    gRunning = 0;
    Object* e = vmTakeException(t);
    if (!e) { setErr("异常 @ %s（未知）", where); return; }
    char msg[256] = {0};
    Field* f = vmFindField(e->clazz, "message", "Ljava/lang/String;");
    if (f) {
        Slot* fl = (Slot*)(e + 1);
        Object* mo = UNREF(fl[f->slot].ref);
        if (mo) {
            char* m = vmStringToUTF(t, mo);
            if (m) { snprintf(msg, sizeof(msg), "%s", m); free(m); }
        }
    }
    setErr("游戏异常退出 @ %s: %s%s%s", where, e->clazz->name,
           msg[0] ? ": " : "", msg);
}

/* ============ 公共 API ============ */

int j2meStart(const char* gameJarPath, const char* rmsDir,
              int screenW, int screenH) {
    gRunning = 0;
    gLastError[0] = 0;
    gMidletName[0] = 0;
    gMainClass[0] = 0;
    gThread = NULL;
    gMidlet = NULL;
    gDispCls = gCanvasCls = NULL;
    gFCur = gFBuf = gFW = gFH = NULL;

    /* 内置 midp.jar */
    NSString* midpPath = [[NSBundle mainBundle] pathForResource:@"midp" ofType:@"jar"];
    if (!midpPath) { setErr("内置 midp.jar 缺失"); return 1; }
    if (vmInit(midpPath.UTF8String) != 0) { setErr("VM 初始化失败"); return 1; }
    vmSetPlatformHooks(&gHooks);
    if (vmAddJar(gameJarPath) != 0) { setErr("游戏 jar 加载失败（文件损坏?）"); vmShutdown(); return 1; }

    bridgeSetRmsDir(rmsDir);
    bridgeRegisterCommonNatives();
    registerPlatformNatives();
    gThread = vmCurrentThread();

    /* 解析 manifest 主类 */
    size_t len = 0;
    char* mf = (char*)zipReadFile(NULL, "META-INF/MANIFEST.MF", &len);
    if (!mf) mf = (char*)zipReadFile(NULL, "meta-inf/MANIFEST.MF", &len);
    if (!mf) { setErr("不是有效的 J2ME 应用（缺少 MANIFEST.MF）"); vmShutdown(); return 1; }
    char* text = (char*)malloc(len + 1);
    if (!text) { free(mf); vmShutdown(); return 1; }
    memcpy(text, mf, len); text[len] = 0;
    free(mf);
    char name[128] = {0}, cls[256] = {0};
    int ok = parseMidlet1(text, name, sizeof(name), cls, sizeof(cls));
    if (!ok) {
        /* 无 MIDlet-1 时退回 MIDlet-Jar-URL 风格主类？J2ME 规范必须有 MIDlet-n */
        manifestGet(text, "MIDlet-Name", name, sizeof(name));
    }
    free(text);
    if (!ok) { setErr("不是有效的 J2ME 应用（缺少 MIDlet-1 属性）"); vmShutdown(); return 1; }
    snprintf(gMidletName, sizeof(gMidletName), "%s", name[0] ? name : "MIDlet");
    snprintf(gMainClass, sizeof(gMainClass), "%s", cls);

    Class* c = vmFindClass(cls);
    if (!c) { setErr("主类不存在: %s", cls); vmShutdown(); return 1; }

    /* 字段缓存 + 屏幕尺寸注入 */
    gDispCls = vmFindClass("javax/microedition/lcdui/Display");
    gCanvasCls = vmFindClass("javax/microedition/lcdui/Canvas");
    if (gDispCls) {
        gFCur = vmFindField(gDispCls, "current", "Ljavax/microedition/lcdui/Displayable;");
        Method* ss = vmFindMethod(gDispCls, "setScreenSize", "(II)V");
        if (ss) {
            Slot sa[2];
            sa[0].ref = NULL; sa[0].i = screenW;
            sa[1].ref = NULL; sa[1].i = screenH;
            vmCall(gThread, ss, sa);
            vmClearException(gThread);
        }
    }
    if (gCanvasCls) {
        gFBuf = vmFindField(gCanvasCls, "buffer", "[I");
        gFW = vmFindField(gCanvasCls, "bufW", "I");
        gFH = vmFindField(gCanvasCls, "bufH", "I");
    }

    /* new + <init> + startApp */
    gThread->gcDisable++;
    gMidlet = vmNewObjectNoCtor(gThread, c);
    gThread->gcDisable--;
    if (!gMidlet) { setErr("MIDlet 实例分配失败"); vmShutdown(); return 1; }
    vmAddGlobalRef(gMidlet);
    if (vmCallSpecialByName(gThread, gMidlet, cls, "<init>", "()V") != 0) {
        printException(gThread, "<init>"); vmShutdown(); return 2;
    }
    if (vmCallSpecialByName(gThread, gMidlet, cls, "startApp", "()V") != 0) {
        printException(gThread, "startApp"); vmShutdown(); return 2;
    }
    gRunning = 1;
    NSLog(@"[j2me] %s 已启动（主类 %s）", gMidletName, cls);
    return 0;
}

const uint32_t* j2meFrame(int* w, int* h) {
    static uint32_t dummy = 0;
    *w = 0; *h = 0;
    if (!gThread || !gRunning) return NULL;
    Object* cur = (gDispCls && gFCur) ? UNREF(gDispCls->statics[gFCur->slot].ref) : NULL;
    if (!cur || !vmIsSubclass(cur->clazz, gCanvasCls)) return &dummy;
    Method* df = vmFindMethodImpl(cur->clazz, "doFrame", "()V");
    if (df) {
        Slot da[1];
        da[0].ref = JREF(cur);
        vmCall(gThread, df, da);
        if (vmHasException(gThread)) { printException(gThread, "doFrame/paint"); return NULL; }
    }
    /* 读 Canvas 像素缓冲 */
    Slot* fl = (Slot*)(cur + 1);
    ArrayObject* buf = (gFBuf && cur) ? (ArrayObject*)UNREF(fl[gFBuf->slot].ref) : NULL;
    int cw = gFW ? (int)fl[gFW->slot].i : 0;
    int ch = gFH ? (int)fl[gFH->slot].i : 0;
    if (!buf || cw <= 0 || ch <= 0) return &dummy;
    *w = cw; *h = ch;
    return (const uint32_t*)ARRAY_DATA(buf);
}

void j2meKey(int keyCode, int down) {
    if (!gThread || !gRunning) return;
    Object* cur = (gDispCls && gFCur) ? UNREF(gDispCls->statics[gFCur->slot].ref) : NULL;
    if (!cur || !vmIsSubclass(cur->clazz, gCanvasCls)) return;
    const char* name = down == 1 ? "keyPressed"
                     : down == 0 ? "keyReleased" : "keyRepeated";
    Method* m = vmFindMethodImpl(cur->clazz, name, "(I)V");
    if (!m) return;
    Slot ka[2];
    ka[0].ref = JREF(cur);
    ka[1].ref = NULL;
    ka[1].i = (int64_t)(int32_t)keyCode;
    vmCall(gThread, m, ka);
    if (vmHasException(gThread)) printException(gThread, name);
}

void j2mePause(void) {
    if (!gThread || !gRunning || !gMainClass[0]) return;
    if (vmCallSpecialByName(gThread, gMidlet, gMainClass, "pauseApp", "()V") != 0)
        printException(gThread, "pauseApp");
}

void j2meResume(void) {
    if (!gThread || !gRunning || !gMainClass[0]) return;
    if (vmCallSpecialByName(gThread, gMidlet, gMainClass, "startApp", "()V") != 0)
        printException(gThread, "startApp");
}

void j2meDestroy(void) {
    if (!gThread) return;
    if (gMidlet) {
        /* destroyApp(true)，异常忽略 */
        Method* dz = vmFindMethodImpl(gMidlet->clazz, "destroyApp", "(Z)V");
        if (dz) {
            Slot da[2];
            da[0].ref = JREF(gMidlet);
            da[1].ref = NULL;
            da[1].i = 1;
            vmCall(gThread, dz, da);
            vmClearException(gThread);
        }
    }
    gRunning = 0;
    gThread = NULL;
    gMidlet = NULL;
    vmShutdown();
    NSLog(@"[j2me] 已销毁");
}

int         j2meIsRunning(void)    { return gRunning; }
const char* j2meLastError(void)    { return gLastError; }
const char* j2meMidletName(void)   { return gMidletName; }
