/* headless_main.c - Windows 本机 headless 测试入口
 * 用法:
 *   headless <midp.jar> [game.jar] [main-class]                        -- main 模式
 *   headless <midp.jar> <game.jar> --midlet <cls> [选项]               -- MIDlet 帧循环模式
 * 选项:
 *   --frames N     帧数（默认 60）
 *   --keys <seq>   按键序列（数字字符，如 2486 表示 上左下右，每帧一键）
 *   --ppm <file>   周期输出 Canvas 帧到 PPM（每 10 帧与最后一帧）
 *   --rms <dir>    RMS 持久化目录（默认 rms）
 */
#include "vm.h"
#include "bridge_common.h"
#include <errno.h>

/* zip.c 提供 */
uint8_t* zipReadFile(const char* jarPath, const char* entryName, size_t* outLen);

/* ============ headless 平台 natives ============ */

static Slot n_noop(VMThread* t, Slot* a) { (void)t; (void)a; Slot r; r.ref = NULL; return r; }

static Slot n_vibrate(VMThread* t, Slot* a) {
    (void)t; (void)a;
    Slot r; r.ref = NULL; r.i = 0;
    return r;
}

static Slot n_midletNotify(VMThread* t, Slot* a) {
    vmLog("[midlet] notify state=%d", (int)a[0].i);
    Slot r; r.ref = NULL;
    return r;
}

/* 读 game jar 的 META-INF/MANIFEST.MF 属性 */
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
            /* 逐行 "Key: Value"（键不分大小写） */
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
                    if (strcmp(line, key) == 0 || strcasecmp(line, key) == 0) {
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

/* headless 无网络：返回 null，Java 层抛 IOException */
static Slot n_httpExecute(VMThread* t, Slot* a) {
    (void)t; (void)a;
    vmLog("[http] headless 模式不支持网络");
    Slot r; r.ref = NULL;
    return r;
}

/* ============ headless 媒体平台：create 落盘 WAV 验证 ============ */

typedef struct {
    int used;
    long durationMs;
    int playing;
    int volume;
} HM;
static HM hm[64];
static int hmNextHandle = 1;

static int hCreate(const void* wav, size_t len, int autoClose) {
    (void)autoClose;
    char path[128];
    snprintf(path, sizeof(path), "out_media_%d.wav", hmNextHandle);
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    if (len) fwrite(wav, 1, len, f);
    fclose(f);
    /* WAV 头解析时长（44 字节标准头，data 长度 @40，采样率 @24，16bit mono） */
    long ms = -1;
    const uint8_t* p = (const uint8_t*)wav;
    if (len >= 44) {
        uint32_t dataLen = (uint32_t)p[40] | ((uint32_t)p[41] << 8) | ((uint32_t)p[42] << 16) | ((uint32_t)p[43] << 24);
        uint32_t rate = (uint32_t)p[24] | ((uint32_t)p[25] << 8) | ((uint32_t)p[26] << 16) | ((uint32_t)p[27] << 24);
        if (rate > 0 && dataLen > 0) ms = (long)(dataLen / 2) * 1000L / (long)rate;
    }
    int h = hmNextHandle++;
    if (h < 64) {
        hm[h].used = 1; hm[h].durationMs = ms; hm[h].playing = 0; hm[h].volume = 100;
    }
    vmLog("[media] create h=%d -> %s (%d bytes, %ld ms)", h, path, (int)len, ms);
    return h;
}
static int hStart(int h) {
    if (h < 0 || h >= 64 || !hm[h].used) return -1;
    hm[h].playing = 1;
    vmLog("[media] start h=%d", h);
    return 0;
}
static int hStop(int h) {
    if (h < 0 || h >= 64 || !hm[h].used) return -1;
    hm[h].playing = 0;
    return 0;
}
static void hClose(int h) {
    if (h < 0 || h >= 64) return;
    hm[h].used = 0; hm[h].playing = 0;
}
static int hPrefetch(int h) { (void)h; return 0; }
static void hSetVolume(int h, int v) { if (h >= 0 && h < 64 && hm[h].used) hm[h].volume = v; }
static int hVolume(int h) { return (h >= 0 && h < 64 && hm[h].used) ? hm[h].volume : -1; }
static int hState(int h) {
    if (h < 0 || h >= 64 || !hm[h].used) return -1;
    return hm[h].playing ? 1 : 0;
}
static long hDuration(int h) { return (h >= 0 && h < 64 && hm[h].used) ? hm[h].durationMs : -1; }
static long hPosition(int h) { return (h >= 0 && h < 64 && hm[h].used && hm[h].playing) ? 0 : 0; }
static void hSetLoop(int h, int c) { vmLog("[media] loop h=%d count=%d", h, c); }
static void hPauseAll(void) {}
static void hResumeAll(void) {}
static void hStopAll(void) {}

static MediaPlatform headlessMedia = {
    hCreate, hStart, hStop, hClose, hPrefetch,
    hSetVolume, hVolume, hState, hDuration, hPosition,
    hSetLoop, hPauseAll, hResumeAll, hStopAll
};

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

/* ============ MIDlet 帧循环模式 ============ */

static void printException(VMThread* t, const char* where) {
    Object* e = vmTakeException(t);
    vmLog("[midlet] 异常 @ %s: %s", where, e ? e->clazz->name : "?");
    if (e) {
        /* 打印 message 字段 */
        Field* f = vmFindField(e->clazz, "message", "Ljava/lang/String;");
        if (f) {
            Slot* fl = (Slot*)(e + 1);
            Object* msg = UNREF(fl[f->slot].ref);
            if (msg) {
                char* m = vmStringToUTF(t, msg);
                if (m) { vmLog("[midlet] message: %s", m); free(m); }
            }
        }
    }
}

static void writePpm(const char* path, Object* canvas, Field* fBuf, Field* fW, Field* fH) {
    if (!canvas || !fBuf) return;
    Slot* fl = (Slot*)(canvas + 1);
    ArrayObject* buf = (ArrayObject*)UNREF(fl[fBuf->slot].ref);
    int w = (int)fl[fW->slot].i, h = (int)fl[fH->slot].i;
    if (!buf || w <= 0 || h <= 0) return;
    uint8_t* img = (uint8_t*)malloc((size_t)w * h * 3);
    if (!img) return;
    int32_t* px = (int32_t*)ARRAY_DATA(buf);
    for (int i = 0; i < w * h; i++) {
        img[i*3]   = (uint8_t)((px[i] >> 16) & 0xFF);
        img[i*3+1] = (uint8_t)((px[i] >> 8) & 0xFF);
        img[i*3+2] = (uint8_t)(px[i] & 0xFF);
    }
    FILE* f = fopen(path, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        fwrite(img, 1, (size_t)w * h * 3, f);
        fclose(f);
        vmLog("[ppm] %dx%d -> %s", w, h, path);
    } else {
        vmLog("[ppm] 写入失败 %s: %s", path, strerror(errno));
    }
    free(img);
}

static int runMidlet(const char* midpJar, const char* gameJar, const char* cls,
                     int frames, const char* keys, const char* ppm, const char* rmsDir) {
    if (vmInit(midpJar) != 0) { vmLog("[midlet] vmInit 失败"); return 1; }
    if (gameJar && vmAddJar(gameJar) != 0) { vmLog("[midlet] game jar 加载失败"); return 1; }
    bridgeSetRmsDir(rmsDir);
    bridgeRegisterCommonNatives();
    registerPlatformNatives();
    bridgeSetMediaPlatform(&headlessMedia);

    VMThread* t = vmCurrentThread();
    Class* c = vmFindClass(cls);
    if (!c) { vmLog("[midlet] 类不存在 %s", cls); return 1; }

    /* new -> <init> -> startApp */
    t->gcDisable++;
    Object* inst = vmNewObjectNoCtor(t, c);
    t->gcDisable--;
    if (!inst) { vmLog("[midlet] 实例分配失败"); return 1; }
    vmAddGlobalRef(inst);
    if (vmCallSpecialByName(t, inst, cls, "<init>", "()V") != 0) {
        printException(t, "<init>");
        return 2;
    }
    if (vmCallSpecialByName(t, inst, cls, "startApp", "()V") != 0) {
        printException(t, "startApp");
        return 2;
    }
    vmLog("[midlet] %s 已启动，帧循环 %d 帧", cls, frames);

    Class* dispCls = vmFindClass("javax/microedition/lcdui/Display");
    Class* canvasCls = vmFindClass("javax/microedition/lcdui/Canvas");
    Field* fCur = dispCls ? vmFindField(dispCls, "current",
                                        "Ljavax/microedition/lcdui/Displayable;") : NULL;
    Field* fBuf = canvasCls ? vmFindField(canvasCls, "buffer", "[I") : NULL;
    Field* fW = canvasCls ? vmFindField(canvasCls, "bufW", "I") : NULL;
    Field* fH = canvasCls ? vmFindField(canvasCls, "bufH", "I") : NULL;

    int keyIdx = 0;
    int paintedFrames = 0;
    for (int fno = 0; fno < frames; fno++) {
        Object* cur = (dispCls && fCur) ? UNREF(dispCls->statics[fCur->slot].ref) : NULL;
        /* 键事件（发给当前 Canvas） */
        if (keys && keys[keyIdx] && cur && vmIsSubclass(cur->clazz, canvasCls)) {
            int kc = (uint8_t)keys[keyIdx++];
            Method* kp = vmFindMethodImpl(cur->clazz, "keyPressed", "(I)V");
            Method* kr = vmFindMethodImpl(cur->clazz, "keyReleased", "(I)V");
            Slot ka[2];
            ka[0].ref = JREF(cur);
            ka[1].i = (int64_t)(int32_t)kc;
            if (kp) { vmCall(t, kp, ka); if (vmHasException(t)) { printException(t, "keyPressed"); return 3; } }
            if (kr) { vmCall(t, kr, ka); if (vmHasException(t)) { printException(t, "keyReleased"); return 3; } }
        }
        /* 帧：Canvas.doFrame()（dirty 则 paint） */
        cur = (dispCls && fCur) ? UNREF(dispCls->statics[fCur->slot].ref) : NULL;
        if (cur && vmIsSubclass(cur->clazz, canvasCls)) {
            Method* df = vmFindMethodImpl(cur->clazz, "doFrame", "()V");
            if (df) {
                Slot da[1];
                da[0].ref = JREF(cur);
                vmCall(t, df, da);
                if (vmHasException(t)) { printException(t, "doFrame/paint"); return 3; }
                paintedFrames++;
            }
        }
        if (vmHasException(t)) { printException(t, "帧循环"); return 3; }
        /* PPM 快照 */
        if (ppm && fBuf && cur && vmIsSubclass(cur->clazz, canvasCls)
            && (fno % 10 == 9 || fno == frames - 1)) {
            writePpm(ppm, cur, fBuf, fW, fH);
        }
    }

    /* destroyApp(true) */
    Method* dz = vmFindMethodImpl(c, "destroyApp", "(Z)V");
    if (dz) {
        Slot da[2];
        da[0].ref = JREF(inst);
        da[1].i = 1;
        vmCall(t, dz, da);
        if (vmHasException(t)) printException(t, "destroyApp");
    }
    vmLog("[midlet] 完成，绘制帧数 %d/%d", paintedFrames, frames);
    vmShutdown();
    return 0;
}

/* ============ main ============ */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: headless <midp.jar> [game.jar] [main-class]\n"
                        "       headless <midp.jar> <game.jar> --midlet <cls> "
                        "[--frames N] [--keys 2486] [--ppm out.ppm] [--rms dir]\n");
        return 1;
    }
    /* MIDlet 模式 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--midlet") == 0 && i + 1 < argc) {
            const char* cls = argv[i + 1];
            int frames = 60;
            const char* keys = NULL, * ppm = NULL, * rmsDir = "rms";
            for (int j = i + 2; j < argc - 1; j++) {
                if (strcmp(argv[j], "--frames") == 0) frames = atoi(argv[j + 1]);
                else if (strcmp(argv[j], "--keys") == 0) keys = argv[j + 1];
                else if (strcmp(argv[j], "--ppm") == 0) ppm = argv[j + 1];
                else if (strcmp(argv[j], "--rms") == 0) rmsDir = argv[j + 1];
            }
            if (argc < 4) { fprintf(stderr, "--midlet 需要 game.jar\n"); return 1; }
            return runMidlet(argv[1], argv[2], cls, frames, keys, ppm, rmsDir);
        }
    }

    /* main 模式（原行为） */
    vmLog("[main] VM init ...");
    if (vmInit(argv[1]) != 0) {
        fprintf(stderr, "[main] vmInit failed\n");
        return 1;
    }
    bridgeRegisterCommonNatives();
    registerPlatformNatives();
    bridgeSetMediaPlatform(&headlessMedia);
    if (argc >= 3 && argv[2][0] && vmAddJar(argv[2]) != 0) {
        fprintf(stderr, "[main] game jar load failed\n");
        return 1;
    }
    if (argc >= 4 && argv[3][0]) {
        VMThread* t = vmCurrentThread();
        Class* c = vmFindClass(argv[3]);
        if (!c) { vmLog("[main] 类不存在 %s", argv[3]); vmShutdown(); return 1; }
        Method* m = vmFindMethod(c, "main", "([Ljava/lang/String;)V");
        if (!m) { vmLog("[main] main 方法不存在 @ %s", argv[3]); vmShutdown(); return 1; }
        Class* sac = vmFindArrayClass("[Ljava/lang/String;");
        ArrayObject* arr = vmNewArray(t, sac, 0);
        Slot args[1];
        args[0].ref = JREF((Object*)arr);
        vmCall(t, m, args);
        if (vmHasException(t)) {
            Object* e = vmTakeException(t);
            vmLog("[main] 未捕获异常 %s", e ? e->clazz->name : "?");
            vmShutdown();
            return 2;
        }
    }
    vmLog("[main] done");
    vmShutdown();
    return 0;
}
