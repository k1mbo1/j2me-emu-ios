/* iOSBridgeCore.c - iOS 平台 Bridge 核心（纯 C，无 ObjC——vm.h 的 Class 与 objc Class 冲突故拆分）
 * MIDlet 运行时 + 平台 natives。http 走 HttpIos（ObjC 隔离层）。
 * 所有接口主线程调用。
 */
#include "iOSBridge.h"
#include "HttpIos.h"
#include "AudioIos.h"
#include "vm.h"
#include "bridge_common.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <AudioToolbox/AudioToolbox.h>
#include "miniz/miniz.h"

/* VM 层 zip.c 提供 */
uint8_t* zipReadFile(const char* jarPath, const char* entryName, size_t* outLen);

/* ============ 全局状态 ============ */
static VMThread* gThread = NULL;
static Object*   gMidlet = NULL;
static int       gRunning = 0;
static char      gLastError[512] = {0};
static char      gMidletName[128] = {0};
static char      gMainClass[256] = {0};

/* 交付验证标记：非 static 导出符号，strings 检查二进制含此串
 * 即可确认该版本源码确实编进了安装包（防止"改了没生效"） */
const char* j2meBuildId(void) { return "J2ME-EMU-BUILD-1.0.13"; }

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
    fprintf(stderr, "[j2me] ERROR: %s\n", gLastError);
}

/* ============ 平台 hooks：日志走 stderr ============ */
static void logHook(const char* msg) { fprintf(stderr, "[j2me] %s\n", msg); }
static void midletStateHook(int state) {
    fprintf(stderr, "[j2me] midlet state=%d\n", state);
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
    fprintf(stderr, "[j2me] midletNotify state=%d\n", (int)a[0].i);
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

/* http: 委托 HttpIos（ObjC 隔离层）。
 * 参数: (url, method, headers "k\r\nv\r\n", body) -> byte[] 或 null(失败)
 * 返回打包: code(u2) msgLen(u2) msg hdrLen(u4) hdr bodyLen(u4) body（均大端） */
static Slot n_httpExecute(VMThread* t, Slot* a) {
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

    HttpIosResult hr;
    int rc = httpIosPerform(url, method, headers,
                            body ? (const uint8_t*)ARRAY_DATA(body) : NULL,
                            body ? (size_t)body->length : 0, &hr);
    free(url); free(method); free(headers);
    if (rc != 0) {
        fprintf(stderr, "[j2me] http 请求失败\n");
        return r;
    }

    size_t msgLen = hr.msg ? strlen(hr.msg) : 0;
    size_t hdrLen = hr.headers ? strlen(hr.headers) : 0;
    size_t total = 2 + 2 + msgLen + 4 + hdrLen + 4 + hr.bodyLen;

    t->gcDisable++;
    ArrayObject* out = vmNewTypeArray(t, EK_BYTE, (int32_t)total);
    t->gcDisable--;
    if (!out) { httpIosFree(&hr); return r; }
    uint8_t* p = (uint8_t*)ARRAY_DATA(out);
    *p++ = (uint8_t)(hr.code >> 8); *p++ = (uint8_t)hr.code;
    *p++ = (uint8_t)(msgLen >> 8); *p++ = (uint8_t)msgLen;
    memcpy(p, hr.msg ? hr.msg : "", msgLen); p += msgLen;
    uint32_t hl = (uint32_t)hdrLen;
    *p++ = (uint8_t)(hl >> 24); *p++ = (uint8_t)(hl >> 16);
    *p++ = (uint8_t)(hl >> 8);  *p++ = (uint8_t)hl;
    memcpy(p, hr.headers ? hr.headers : "", hdrLen); p += hdrLen;
    uint32_t bl = (uint32_t)hr.bodyLen;
    *p++ = (uint8_t)(bl >> 24); *p++ = (uint8_t)(bl >> 16);
    *p++ = (uint8_t)(bl >> 8);  *p++ = (uint8_t)bl;
    if (hr.bodyLen) memcpy(p, hr.body, hr.bodyLen);
    httpIosFree(&hr);

    r.ref = JREF((Object*)out);
    return r;
}

/* MMAPI 媒体平台：AVAudioPlayer（AudioIos） */
static int mp_create(const void* wav, size_t len, int autoClose) { return audioIosCreate(wav, len, autoClose); }
static int mp_start(int h) { return audioIosStart(h); }
static int mp_stop(int h)  { return audioIosStop(h); }
static void mp_close(int h) { audioIosClose(h); }
static int mp_prefetch(int h) { return audioIosPrefetch(h); }
static void mp_setVolume(int h, int v) { audioIosSetVolume(h, v); }
static int mp_volume(int h) { return audioIosVolume(h); }
static int mp_state(int h) { return audioIosState(h); }
static long mp_duration(int h) { return audioIosDurationMs(h); }
static long mp_position(int h) { return audioIosPositionMs(h); }
static void mp_setLoop(int h, int c) { audioIosSetLoop(h, c); }
static void mp_pauseAll(void) { audioIosPauseAll(); }
static void mp_resumeAll(void) { audioIosResumeAll(); }
static void mp_stopAll(void) { audioIosStopAll(); }

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

/* ============ manifest 解析（已在 Windows parse_mf_test 验证） ============ */

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
    const char* p1 = strchr(m1, ',');
    if (!p1) return 0;
    const char* p2 = strchr(p1 + 1, ',');
    if (!p2) return 0;
    size_t nl = (size_t)(p1 - m1);
    if (nl >= nameCap) nl = nameCap - 1;
    memcpy(name, m1, nl); name[nl] = 0;
    const char* cs = p2 + 1;
    while (*cs == ' ') cs++;
    if (cs[0] == '.' && cs[1] == '/') cs += 2;
    size_t cl = strlen(cs);
    while (cl > 0 && (cs[cl-1] == ' ' || cs[cl-1] == '\r')) cl--;
    if (cl == 0 || cl >= clsCap) return 0;
    memcpy(cls, cs, cl); cls[cl] = 0;
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

int j2meStart(const char* midpJarPath, const char* gameJarPath, const char* rmsDir,
              int screenW, int screenH) {
    fprintf(stderr, "[j2me] build %s\n", j2meBuildId());   /* 保符号存活 + e2e 日志可查 */
    gRunning = 0;
    gLastError[0] = 0;
    gMidletName[0] = 0;
    gMainClass[0] = 0;
    gThread = NULL;
    gMidlet = NULL;
    gDispCls = NULL;
    gCanvasCls = NULL;
    gFCur = gFBuf = gFW = gFH = NULL;

    if (!midpJarPath || vmInit(midpJarPath) != 0) { setErr("VM 初始化失败（midp.jar 缺失）"); return 1; }
    vmSetPlatformHooks(&gHooks);
    if (gameJarPath && vmAddJar(gameJarPath) != 0) {
        setErr("游戏 jar 加载失败（文件损坏?）");
        vmShutdown();
        return 1;
    }

    bridgeSetRmsDir(rmsDir ? rmsDir : "");
    {
        static MediaPlatform iosMedia = {
            mp_create, mp_start, mp_stop, mp_close, mp_prefetch,
            mp_setVolume, mp_volume, mp_state, mp_duration, mp_position,
            mp_setLoop, mp_pauseAll, mp_resumeAll, mp_stopAll
        };
        bridgeSetMediaPlatform(&iosMedia);
    }
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
    fprintf(stderr, "[j2me] %s 已启动（主类 %s）\n", gMidletName, cls);
    return 0;
}

static void keyPump(void);

const uint32_t* j2meFrame(int* w, int* h) {
    static uint32_t dummy = 0;
    *w = 0; *h = 0;
    if (!gThread || !gRunning) return NULL;
    keyPump();
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
    ArrayObject* buf = gFBuf ? (ArrayObject*)UNREF(fl[gFBuf->slot].ref) : NULL;
    int cw = gFW ? (int)fl[gFW->slot].i : 0;
    int ch = gFH ? (int)fl[gFH->slot].i : 0;
    if (!buf || cw <= 0 || ch <= 0) return &dummy;
    *w = cw; *h = ch;
    return (const uint32_t*)ARRAY_DATA(buf);
}

/* ============ 按键事件队列 ============
 * UI 线程只入队，帧驱动（display link -> j2meFrame）开头统一冲刷：
 * 按键与其他 VM 调用串行化在同一上下文，消除跨线程并发进 VM
 * 导致的按键丢失/解释器状态竞争 */
typedef struct { int code, down; } J2KeyEvent;
#define J2KEYQ_CAP 128
static J2KeyEvent gKeyQ[J2KEYQ_CAP];
static atomic_int gKeyQWr = 0;   /* 写位（仅 UI 线程推进） */
static int gKeyQRd = 0;          /* 读位（仅帧线程推进） */
static int gKeyDispatched = 0;   /* 已派发到游戏的按键事件累计数（自检用） */

int j2meKeyDispatched(void) { return gKeyDispatched; }

void j2meKey(int keyCode, int down) {
    if (!gThread || !gRunning) return;
    int wr = atomic_load(&gKeyQWr);
    int next = (wr + 1) % J2KEYQ_CAP;
    if (next == gKeyQRd) return;   /* 队列满：丢弃（防按键风暴） */
    gKeyQ[wr].code = keyCode;
    gKeyQ[wr].down = down;
    atomic_store(&gKeyQWr, next);
    if (getenv("J2ME_KEYLOG"))
        fprintf(stderr, "[key] enq code=%d down=%d wr=%d\n", keyCode, down, next);
}

/* 在帧驱动上下文调用：把队列中的按键依次派发给当前 Canvas */
static void keyPump(void) {
    int wr = atomic_load(&gKeyQWr);
    if (gKeyQRd == wr) return;
    Object* cur = (gDispCls && gFCur) ? UNREF(gDispCls->statics[gFCur->slot].ref) : NULL;
    if (!cur || !vmIsSubclass(cur->clazz, gCanvasCls)) {
        /* Canvas 未就绪（游戏正在切换 Displayable）：保留事件到下一帧，
         * 不能静默丢弃——否则切画面瞬间的按键全部失效 */
        if (getenv("J2ME_KEYLOG"))
            fprintf(stderr, "[key] pending %d event(s), canvas not ready\n", (wr - gKeyQRd + J2KEYQ_CAP) % J2KEYQ_CAP);
        return;
    }
    while (gKeyQRd != wr && gRunning) {
        J2KeyEvent ev = gKeyQ[gKeyQRd];
        const char* name = ev.down == 1 ? "keyPressed"
                         : ev.down == 0 ? "keyReleased" : "keyRepeated";
        Method* m = vmFindMethodImpl(cur->clazz, name, "(I)V");
        if (m) {
            Slot ka[2];
            ka[0].ref = JREF(cur);
            ka[1].ref = NULL;
            ka[1].i = (int64_t)(int32_t)ev.code;
            vmCall(gThread, m, ka);
            gKeyDispatched++;
            if (getenv("J2ME_KEYLOG"))
                fprintf(stderr, "[key] dispatch %s(%d) rd=%d\n", name, ev.code, gKeyQRd);
            if (vmHasException(gThread)) { printException(gThread, name); break; }
        } else if (getenv("J2ME_KEYLOG")) {
            fprintf(stderr, "[key] NO handler %s on %s\n", name, cur->clazz->name);
        }
        gKeyQRd = (gKeyQRd + 1) % J2KEYQ_CAP;
    }
}

void j2mePause(void) {
    audioIosPauseAll();
    if (!gThread || !gRunning || !gMainClass[0]) return;
    if (vmCallSpecialByName(gThread, gMidlet, gMainClass, "pauseApp", "()V") != 0)
        printException(gThread, "pauseApp");
}

void j2meResume(void) {
    if (!gThread || !gRunning || !gMainClass[0]) return;
    if (vmCallSpecialByName(gThread, gMidlet, gMainClass, "startApp", "()V") != 0)
        printException(gThread, "startApp");
    audioIosResumeAll();
}

void j2meDestroy(void) {
    audioIosStopAll();
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
    /* 清空按键队列：重启/换游戏后不能带着上一次的残留事件 */
    atomic_store(&gKeyQWr, 0);
    gKeyQRd = 0;
    vmShutdown();
    fprintf(stderr, "[j2me] 已销毁\n");
}

int         j2meIsRunning(void)    { return gRunning; }
const char* j2meLastError(void)    { return gLastError; }
const char* j2meMidletName(void)   { return gMidletName; }

/* 不启动 VM 直接读 jar 的 MIDlet-1 显示名（游戏库列表用，miniz 独立打开） */
int j2mePeekManifest(const char* jarPath, char* nameOut, size_t cap) {
    if (!jarPath || !nameOut || cap == 0) return -1;
    nameOut[0] = 0;
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, jarPath, 0)) return -1;
    int idx = mz_zip_reader_locate_file(&zip, "META-INF/MANIFEST.MF", NULL, 0);
    if (idx < 0) idx = mz_zip_reader_locate_file(&zip, "meta-inf/MANIFEST.MF", NULL, 0);
    if (idx < 0) { mz_zip_reader_end(&zip); return -1; }
    size_t sz = 0;
    char* mf = (char*)mz_zip_reader_extract_to_heap(&zip, idx, &sz, 0);
    mz_zip_reader_end(&zip);
    if (!mf) return -1;
    char* text = (char*)malloc(sz + 1);
    if (!text) { free(mf); return -1; }
    memcpy(text, mf, sz);
    text[sz] = 0;
    free(mf);
    char name[128] = {0}, cls[256] = {0};
    int ok = parseMidlet1(text, name, sizeof(name), cls, sizeof(cls));
    free(text);
    if (!ok || !name[0]) return -1;
    snprintf(nameOut, cap, "%s", name);
    return 0;
}

void j2meSetVolume(int vol) { audioIosSetMasterVolume(vol); }
int  j2meVolume(void)       { return audioIosMasterVolume(); }
