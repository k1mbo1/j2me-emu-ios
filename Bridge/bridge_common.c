/* bridge_common.c - 平台无关 Bridge natives（headless Windows / iOS 共用）
 * 1. rasterText: 8x12 点阵文本光栅（font8x12.h）
 * 2. bridgeReadResource: jar 内资源读取
 * 3. decodePng: PNG -> Image（支持 8bit 灰/灰A/RGB/RGBA + 1/2/4/8bit palette）
 * 4. RMS 文件持久化（<rmsDir>/<name>.rms + _index.txt 索引）
 * 纪律：native 执行期间解释器已 gcDisable，C 栈引用安全
 */
#include "bridge_common.h"
#include "font8x12.h"
#include "cjk_font.h"
#include "miniz/miniz.h"
#include "midi_synth.h"
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#endif

/* zip.c 提供 */
uint8_t* zipReadFile(const char* jarPath, const char* entryName, size_t* outLen);

/* ============ RMS 目录 ============ */

static char g_rmsDir[512] = "rms";

void bridgeSetRmsDir(const char* dir) {
    if (dir) strncpy(g_rmsDir, dir, sizeof(g_rmsDir) - 1);
}

static void ensureRmsDir(void) {
#ifdef _WIN32
    _mkdir(g_rmsDir);
#else
    mkdir(g_rmsDir, 0755);
#endif
}

static int safeStoreName(const char* n) {
    if (!n || !*n || strlen(n) > 200) return 0;
    for (const char* p = n; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' ||
            *p == '"' || *p == '<' || *p == '>' || *p == '|')
            return 0;
    return 1;
}

/* ============ rasterText ============ */

/* Graphics.rasterText(int[] data, int stride, int x, int y, int color,
 *                     int clipX, int clipY, int clipW, int clipH, String s, int bold) */
static Slot n_rasterText(VMThread* t, Slot* a) {
    (void)t;
    Slot r;
    r.ref = NULL;
    ArrayObject* data = (ArrayObject*)UNREF(a[0].ref);
    Object* s = UNREF(a[9].ref);
    if (!data) return r;
    int stride = (int)a[1].i, x = (int)a[2].i, y = (int)a[3].i, color = (int)a[4].i;
    int clipX = (int)a[5].i, clipY = (int)a[6].i, clipW = (int)a[7].i, clipH = (int)a[8].i;
    int bold = (int)a[10].i;
    int32_t* px = (int32_t*)ARRAY_DATA(data);
    int len = s ? vmStringLength(s) : 0;
    uint16_t* ch = len > 0 ? vmStringChars(s) : NULL;
    int penX = x;
    for (int i = 0; i < len; i++) {
        uint16_t c = ch[i];
        /* 宽字符：交给平台字体栅格化为 12x12 覆盖率（中文/全角）。
         * 二值化硬边渲染（覆盖率>=50% 实色）：配合整数倍缩放，
         * 每个字形像素是干净色块，与真机 S40 点阵字观感一致；
         * 抗锯齿灰度经放大后边缘发灰发糊，弃用。无字形时退回 '?'。 */
        if (emuIsWideChar(c)) {
            unsigned char glyph[144];
            if (emuCjkGlyph(c, glyph)) {
                for (int row = 0; row < 12; row++) {
                    int py = y + row;
                    if (py < clipY || py >= clipY + clipH) continue;
                    if (py < 0) continue;
                    if (py * stride >= data->length) continue;
                    for (int col = 0; col < 12; col++) {
                        int a = glyph[row * 12 + col];
                        /* bold：向右膨胀 1px（与 ASCII 的右邻复制语义一致） */
                        if (bold && col > 0 && glyph[row * 12 + col - 1] > a)
                            a = glyph[row * 12 + col - 1];
                        if (a < 128) continue;      /* 硬边阈值 50% */
                        int pxi = penX + col;
                        if (pxi < clipX || pxi >= clipX + clipW || pxi < 0) continue;
                        if (py * stride + pxi >= data->length) continue;
                        px[py * stride + pxi] = color;
                    }
                }
                penX += 12;
                continue;
            }
        }
        if (c < 32 || c > 126) c = '?';
        const uint8_t* glyph = font8x12[c - 32];
        int gx = penX;
        penX += 8;
        for (int row = 0; row < 12; row++) {
            int py = y + row;
            if (py < clipY || py >= clipY + clipH) continue;
            if (py < 0 || py >= data->length / (stride ? stride : 1)) {
                if (stride == 0) break;
                if (py * stride >= data->length) continue;
            }
            uint8_t bits = glyph[row];
            if (!bits) continue;
            for (int col = 0; col < 8; col++) {
                if (!(bits & (0x80 >> col))) continue;
                int pxi = gx + col;
                if (pxi < clipX || pxi >= clipX + clipW || pxi < 0) continue;
                if (py * stride + pxi >= data->length) continue;
                px[py * stride + pxi] = color;
                if (bold) {
                    int px2 = pxi + 1;
                    if (px2 < clipX + clipW && py * stride + px2 < data->length)
                        px[py * stride + px2] = color;
                }
            }
        }
    }
    return r;
}

/* ============ jar 资源读取 ============ */

/* Image.bridgeReadResource(String name) -> byte[] */
static Slot n_bridgeReadResource(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    Object* s = UNREF(a[0].ref);
    if (!s) { vmThrow(t, "java/lang/NullPointerException", NULL); return r; }
    char* name = vmStringToUTF(t, s);
    if (!name) return r;
    size_t len = 0;
    uint8_t* p = zipReadFile(NULL, name, &len);
    if (!p && name[0] == '/') p = zipReadFile(NULL, name + 1, &len);
    free(name);
    if (!p) return r;   /* Java 层抛 IOException */
    ArrayObject* arr = vmNewTypeArray(t, EK_BYTE, (int32_t)len);
    if (arr && len > 0) memcpy(ARRAY_DATA(arr), p, len);
    free(p);
    if (!arr) { if (vmHasException(t)) vmClearException(t); return r; }
    r.ref = JREF((Object*)arr);
    return r;
}

/* ============ PNG 解码 ============ */

/* Image.decodePng(byte[] data) -> Image（失败返回 null，Java 层抛 IOException） */
static Slot n_decodePng(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    ArrayObject* data = (ArrayObject*)UNREF(a[0].ref);
    if (!data) return r;
    uint8_t* p = (uint8_t*)ARRAY_DATA(data);
    int32_t n = data->length;
    if (n < 8 || memcmp(p, "\x89PNG\r\n\x1a\n", 8) != 0) return r;

    uint32_t w = 0, h = 0;
    int depth = 0, ctype = 0, interlace = 0;
    uint8_t palette[256][3];
    int nPal = 0;
    uint8_t trns[256];
    int nTrns = 0;
    uint8_t* idat = NULL;
    size_t idatLen = 0, idatCap = 0;

    int32_t off = 8;
    while (off + 8 <= n) {
        uint32_t len = ((uint32_t)p[off] << 24) | ((uint32_t)p[off+1] << 16)
                     | ((uint32_t)p[off+2] << 8) | (uint32_t)p[off+3];
        const uint8_t* type = p + off + 4;
        const uint8_t* body = p + off + 8;
        if ((int64_t)off + 12 + (int64_t)len > n) { free(idat); return r; }
        if (memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            w = ((uint32_t)body[0] << 24) | ((uint32_t)body[1] << 16) | ((uint32_t)body[2] << 8) | body[3];
            h = ((uint32_t)body[4] << 24) | ((uint32_t)body[5] << 16) | ((uint32_t)body[6] << 8) | body[7];
            depth = body[8]; ctype = body[9];
            interlace = body[12];
        } else if (memcmp(type, "PLTE", 4) == 0) {
            nPal = (int)(len / 3);
            if (nPal > 256) nPal = 256;
            for (int i = 0; i < nPal; i++) {
                palette[i][0] = body[i*3]; palette[i][1] = body[i*3+1]; palette[i][2] = body[i*3+2];
            }
        } else if (memcmp(type, "tRNS", 4) == 0) {
            nTrns = (int)(len > 256 ? 256 : len);
            memcpy(trns, body, nTrns);
        } else if (memcmp(type, "IDAT", 4) == 0) {
            if (idatLen + len > idatCap) {
                idatCap = (idatLen + len + 4096) * 2;
                uint8_t* nb = (uint8_t*)realloc(idat, idatCap);
                if (!nb) { free(idat); return r; }
                idat = nb;
            }
            memcpy(idat + idatLen, body, len);
            idatLen += len;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        off += 12 + (int32_t)len;
    }

    /* 合法性 */
    if (!w || !h || w > 4096 || h > 4096 || interlace || !idat) { free(idat); return r; }
    if (depth != 8 && ctype != 3) { free(idat); return r; }   /* 非 palette 必须 8bit */
    if (ctype != 0 && ctype != 2 && ctype != 3 && ctype != 4 && ctype != 6) { free(idat); return r; }
    if (ctype == 3 && depth > 8) { free(idat); return r; }

    int ch = (ctype == 2) ? 3 : (ctype == 4) ? 2 : (ctype == 6) ? 4 : 1;
    int bppBits = (ctype == 3) ? depth : depth * ch;
    int bytesPerRow = (int)((w * (uint32_t)bppBits + 7) / 8);
    int bppBytes = (bppBits + 7) / 8;
    if (bppBytes < 1) bppBytes = 1;

    /* zlib 解压 */
    size_t rawLen = (size_t)h * (size_t)(1 + bytesPerRow);
    uint8_t* raw = (uint8_t*)malloc(rawLen ? rawLen : 1);
    if (!raw) { free(idat); return r; }
    {
        mz_ulong dl = (mz_ulong)rawLen;
        if (mz_uncompress(raw, &dl, idat, (mz_ulong)idatLen) != MZ_OK) {
            free(raw); free(idat); return r;
        }
    }
    free(idat);

    /* 逐行反滤波 */
    for (uint32_t y = 0; y < h; y++) {
        uint8_t ft = raw[y * (size_t)(1 + bytesPerRow)];
        uint8_t* line = raw + y * (size_t)(1 + bytesPerRow) + 1;
        uint8_t* prev = y ? (raw + (y - 1) * (size_t)(1 + bytesPerRow) + 1) : NULL;
        for (int x = 0; x < bytesPerRow; x++) {
            int A = (x >= bppBytes) ? line[x - bppBytes] : 0;
            int B = prev ? prev[x] : 0;
            int C = (prev && x >= bppBytes) ? prev[x - bppBytes] : 0;
            int v = line[x];
            switch (ft) {
                case 0: break;
                case 1: v += A; break;
                case 2: v += B; break;
                case 3: v += (A + B) >> 1; break;
                case 4: {
                    int pp = A + B - C;
                    int pa = pp > A ? pp - A : A - pp;
                    int pb = pp > B ? pp - B : B - pp;
                    int pc = pp > C ? pp - C : C - pp;
                    v += (pa <= pb && pa <= pc) ? A : (pb <= pc ? B : C);
                } break;
                default: free(raw); return r;
            }
            line[x] = (uint8_t)v;
        }
    }

    /* 展开 -> ARGB */
    uint32_t* out = (uint32_t*)malloc((size_t)w * h * 4);
    if (!out) { free(raw); return r; }
    for (uint32_t y = 0; y < h; y++) {
        uint8_t* line = raw + y * (size_t)(1 + bytesPerRow) + 1;
        for (uint32_t x = 0; x < w; x++) {
            uint32_t argb;
            if (ctype == 3) {
                int idx = 0;
                if (depth == 8) {
                    idx = line[x];
                } else {
                    uint32_t bitPos = x * (uint32_t)depth;
                    int byteOff = (int)(bitPos >> 3);
                    int shift = 8 - depth - (int)(bitPos & 7);
                    int mask = (1 << depth) - 1;
                    idx = (line[byteOff] >> shift) & mask;
                }
                uint32_t al = (idx < nTrns) ? trns[idx] : 255;
                uint32_t rr = (idx < nPal) ? palette[idx][0] : 0;
                uint32_t gg = (idx < nPal) ? palette[idx][1] : 0;
                uint32_t bb = (idx < nPal) ? palette[idx][2] : 0;
                argb = (al << 24) | (rr << 16) | (gg << 8) | bb;
            } else if (ctype == 0) {
                uint32_t v = line[x];
                argb = 0xFF000000u | (v << 16) | (v << 8) | v;
            } else if (ctype == 2) {
                argb = 0xFF000000u | ((uint32_t)line[x*3] << 16)
                     | ((uint32_t)line[x*3+1] << 8) | (uint32_t)line[x*3+2];
            } else if (ctype == 4) {
                uint32_t v = line[x*2];
                argb = ((uint32_t)line[x*2+1] << 24) | (v << 16) | (v << 8) | v;
            } else {
                argb = ((uint32_t)line[x*4+3] << 24) | ((uint32_t)line[x*4] << 16)
                     | ((uint32_t)line[x*4+1] << 8) | (uint32_t)line[x*4+2];
            }
            out[y * w + x] = argb;
        }
    }
    free(raw);

    /* 创建 Image 对象（不调构造器，直接填字段） */
    t->gcDisable++;
    Class* imgCls = vmFindClass("javax/microedition/lcdui/Image");
    Object* img = imgCls ? vmNewObjectNoCtor(t, imgCls) : NULL;
    ArrayObject* rgbArr = img ? vmNewTypeArray(t, EK_INT, (int32_t)(w * h)) : NULL;
    t->gcDisable--;
    if (!img || !rgbArr) { free(out); if (vmHasException(t)) vmClearException(t); return r; }
    memcpy(ARRAY_DATA(rgbArr), out, (size_t)w * h * 4);
    free(out);
    Field* fRgb = vmFindField(imgCls, "rgb", "[I");
    Field* fW = vmFindField(imgCls, "w", "I");
    Field* fH = vmFindField(imgCls, "h", "I");
    if (!fRgb || !fW || !fH) return r;
    Slot* fl = (Slot*)(img + 1);
    fl[fRgb->slot].ref = JREF((Object*)rgbArr);
    fl[fW->slot].i = (int64_t)(int32_t)w;
    fl[fH->slot].i = (int64_t)(int32_t)h;
    r.ref = JREF(img);
    return r;
}

/* ============ RMS 持久化 ============ */

/* RecordStore.nativeLoadStore(String name) -> byte[]（无则 null） */
static Slot n_loadStore(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    Object* s = UNREF(a[0].ref);
    char* name = s ? vmStringToUTF(t, s) : NULL;
    if (!name || !safeStoreName(name)) { free(name); return r; }
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s.rms", g_rmsDir, name);
    free(name);
    FILE* f = fopen(path, "rb");
    if (!f) return r;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return r; }
    long sz = ftell(f);
    if (sz < 0 || sz > 64 * 1024 * 1024) { fclose(f); return r; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return r; }
    ArrayObject* arr = vmNewTypeArray(t, EK_BYTE, (int32_t)sz);
    if (!arr) { fclose(f); if (vmHasException(t)) vmClearException(t); return r; }
    if (sz > 0 && fread(ARRAY_DATA(arr), 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); return r;
    }
    fclose(f);
    r.ref = JREF((Object*)arr);
    return r;
}

/* RecordStore.nativeSaveStore(String name, byte[] data) */
static Slot n_saveStore(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    Object* s = UNREF(a[0].ref);
    ArrayObject* data = (ArrayObject*)UNREF(a[1].ref);
    char* name = s ? vmStringToUTF(t, s) : NULL;
    if (!name || !safeStoreName(name)) {
        free(name);
        vmThrow(t, "javax/microedition/rms/RecordStoreException", "bad store name");
        return r;
    }
    ensureRmsDir();
    char path[1100], ipath[1100];
    snprintf(path, sizeof(path), "%s/%s.rms", g_rmsDir, name);
    snprintf(ipath, sizeof(ipath), "%s/_index.txt", g_rmsDir);
    FILE* f = fopen(path, "wb");
    if (!f) {
        vmThrow(t, "javax/microedition/rms/RecordStoreException", "cannot write store");
        free(name);
        return r;
    }
    if (data && data->length > 0)
        fwrite(ARRAY_DATA(data), 1, (size_t)data->length, f);
    fclose(f);
    /* 索引去重追加 */
    int found = 0;
    FILE* gi = fopen(ipath, "r");
    if (gi) {
        char ln[300];
        while (fgets(ln, sizeof(ln), gi)) {
            size_t L = strlen(ln);
            while (L && (ln[L-1] == '\n' || ln[L-1] == '\r')) ln[--L] = 0;
            if (strcmp(ln, name) == 0) { found = 1; break; }
        }
        fclose(gi);
    }
    if (!found) {
        FILE* ai = fopen(ipath, "a");
        if (ai) { fprintf(ai, "%s\n", name); fclose(ai); }
    }
    free(name);
    return r;
}

/* RecordStore.nativeDeleteStore(String name) -> boolean（存在并删除成功） */
static Slot n_deleteStore(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    r.i = 0;
    Object* s = UNREF(a[0].ref);
    char* name = s ? vmStringToUTF(t, s) : NULL;
    if (!name || !safeStoreName(name)) { free(name); return r; }
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s.rms", g_rmsDir, name);
    int ok = remove(path) == 0;
    if (ok) {
        /* 从 _index.txt 移除该行（写临时文件再替换） */
        char ipath[1100], tpath[1100];
        snprintf(ipath, sizeof(ipath), "%s/_index.txt", g_rmsDir);
        snprintf(tpath, sizeof(tpath), "%s/_index.tmp", g_rmsDir);
        FILE* gi = fopen(ipath, "r");
        if (gi) {
            FILE* go = fopen(tpath, "w");
            if (go) {
                char ln[300];
                while (fgets(ln, sizeof(ln), gi)) {
                    size_t L = strlen(ln);
                    while (L && (ln[L-1] == '\n' || ln[L-1] == '\r')) ln[--L] = 0;
                    if (L && strcmp(ln, name) != 0) fprintf(go, "%s\n", ln);
                }
                fclose(go);
                remove(ipath);
                rename(tpath, ipath);
            }
            fclose(gi);
        }
    }
    free(name);
    r.i = ok ? 1 : 0;
    return r;
}

/* RecordStore.nativeListStores() -> String[] */
static Slot n_listStores(VMThread* t, Slot* a) {
    (void)a;
    Slot r;
    r.ref = NULL;
    char ipath[1100];
    snprintf(ipath, sizeof(ipath), "%s/_index.txt", g_rmsDir);
    char** names = NULL;
    int n = 0, cap = 0;
    FILE* gi = fopen(ipath, "r");
    if (gi) {
        char ln[300];
        while (fgets(ln, sizeof(ln), gi)) {
            size_t L = strlen(ln);
            while (L && (ln[L-1] == '\n' || ln[L-1] == '\r')) ln[--L] = 0;
            if (!L) continue;
            if (n == cap) {
                cap = cap ? cap * 2 : 8;
                char** nn = (char**)realloc(names, (size_t)cap * sizeof(char*));
                if (!nn) break;
                names = nn;
            }
            names[n] = (char*)malloc(L + 1);
            if (!names[n]) break;
            memcpy(names[n], ln, L + 1);
            n++;
        }
        fclose(gi);
    }
    t->gcDisable++;
    Class* strCls = vmFindClass("java/lang/String");
    ArrayObject* arr = strCls ? vmNewObjArray(t, strCls, n) : NULL;
    if (arr) {
        Slot* el = (Slot*)ARRAY_DATA(arr);
        for (int i = 0; i < n; i++) {
            Object* so = vmNewStringUTF(t, names[i]);
            if (!so) break;
            el[i].ref = JREF(so);
        }
    }
    t->gcDisable--;
    for (int i = 0; i < n; i++) free(names[i]);
    free(names);
    if (!arr) { if (vmHasException(t)) vmClearException(t); return r; }
    r.ref = JREF((Object*)arr);
    return r;
}

/* ============ 注册 ============ */

/* ============ 媒体（MMAPI 音效）============ */

static MediaPlatform g_media;

void bridgeSetMediaPlatform(const MediaPlatform* mp) {
    if (mp) g_media = *mp;
    else memset(&g_media, 0, sizeof(g_media));
}

/* type 判定（大小写不敏感子串） */
static int typeHas(const char* type, const char* what) {
    if (!type) return 0;
    size_t wl = strlen(what);
    for (const char* p = type; *p; p++) {
        size_t k = 0;
        while (k < wl && p[k]) {
            char a = p[k], b = what[k];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
            k++;
        }
        if (k == wl) return 1;
    }
    return 0;
}

/* PlayerImpl.nCreate(byte[] data, String mimeType) -> handle */
static Slot n_mediaCreate(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    r.i = -1;
    ArrayObject* data = (ArrayObject*)UNREF(a[0].ref);
    Object* typeS = UNREF(a[1].ref);
    char* type = typeS ? vmStringToUTF(t, typeS) : NULL;
    if (!data) { free(type); return r; }
    uint8_t* p = (uint8_t*)ARRAY_DATA(data);
    int32_t n = data->length;
    if (n < 4) { free(type); return r; }

    int16_t* pcm = NULL;
    size_t pcmLen = 0;
    int isMidi = typeHas(type, "midi") || (type == NULL && n > 4 && memcmp(p, "MThd", 4) == 0);
    if (isMidi) pcm = synthMidi(p, (size_t)n, &pcmLen);
    else if (typeHas(type, "nokia-tone")) pcm = synthOta(p, (size_t)n, &pcmLen);
    else if (typeHas(type, "tone-seq")) pcm = synthToneSeq(p, (size_t)n, &pcmLen);
    free(type);
    if (pcm) {
        fprintf(stderr, "[media] synth %d samples -> wav\n", (int)pcmLen);
        size_t wavLen = 0;
        uint8_t* wav = pcmToWav(pcm, pcmLen, &wavLen);
        free(pcm);
        if (!wav) return r;
        if (g_media.create) r.i = g_media.create(wav, wavLen, 0);
        free(wav);
        return r;
    }
    /* 非 MIDI/ToneControl：字节流透传（wav/mp3/aac 由平台播放器解） */
    if (g_media.create) r.i = g_media.create(p, (size_t)n, 0);
    return r;
}

/* PlayerImpl.nCreateToneSeq(byte[] seq) -> handle（ToneControl 音序） */
static Slot n_mediaCreateToneSeq(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    r.i = -1;
    ArrayObject* seq = (ArrayObject*)UNREF(a[0].ref);
    if (!seq) return r;
    size_t pcmLen = 0;
    int16_t* pcm = synthToneSeq((const uint8_t*)ARRAY_DATA(seq), (size_t)seq->length, &pcmLen);
    if (!pcm) return r;
    size_t wavLen = 0;
    uint8_t* wav = pcmToWav(pcm, pcmLen, &wavLen);
    free(pcm);
    if (!wav) return r;
    if (g_media.create) r.i = g_media.create(wav, wavLen, 0);
    free(wav);
    return r;
}

/* Manager.nPlayTone(int note, int durMs, int vol) -> 0 成功
 * 非阻塞（VM 跑主线程，阻塞会卡死帧循环；规范允许近似） */
static Slot n_playTone(VMThread* t, Slot* a) {
    (void)t;
    Slot r;
    r.ref = NULL;
    r.i = -1;
    int note = (int)a[0].i, durMs = (int)a[1].i, vol = (int)a[2].i;
    size_t pcmLen = 0;
    int16_t* pcm = synthTone(note, durMs, vol, &pcmLen);
    if (!pcm) return r;
    size_t wavLen = 0;
    uint8_t* wav = pcmToWav(pcm, pcmLen, &wavLen);
    free(pcm);
    if (!wav) return r;
    if (g_media.create) {
        int h = g_media.create(wav, wavLen, 1);   /* autoClose：播完平台自动回收 */
        if (h >= 0 && g_media.start) {
            g_media.start(h);
            r.i = 0;
        }
    }
    free(wav);
    return r;
}

static Slot n_mediaStart(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = (g_media.start && g_media.start((int)a[0].i) == 0) ? 0 : -1;
    return r;
}
static Slot n_mediaStop(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = (g_media.stop && g_media.stop((int)a[0].i) == 0) ? 0 : -1;
    return r;
}
static Slot n_mediaClose(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    if (g_media.close) g_media.close((int)a[0].i);
    return r;
}
static Slot n_mediaPrefetch(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = (g_media.prefetch && g_media.prefetch((int)a[0].i) != 0) ? -1 : 0;
    return r;
}
static Slot n_mediaSetLoop(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    if (g_media.setLoop) g_media.setLoop((int)a[0].i, (int)a[1].i);
    return r;
}
static Slot n_mediaSetVolume(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    if (g_media.setVolume) g_media.setVolume((int)a[0].i, (int)a[1].i);
    return r;
}
static Slot n_mediaGetVolume(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = g_media.volume ? g_media.volume((int)a[0].i) : 100;
    return r;
}
static Slot n_mediaState(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = g_media.state ? g_media.state((int)a[0].i) : 0;
    return r;
}
static Slot n_mediaDuration(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = g_media.durationMs ? g_media.durationMs((int)a[0].i) : -1;
    return r;
}
static Slot n_mediaPosition(VMThread* t, Slot* a) {
    (void)t;
    Slot r; r.ref = NULL;
    r.i = g_media.positionMs ? g_media.positionMs((int)a[0].i) : 0;
    return r;
}
/* Class.nativeGetResource(String name) -> byte[]（getResourceAsStream 后端） */
static Slot n_classGetResource(VMThread* t, Slot* a) {
    Slot r;
    r.ref = NULL;
    Object* s = UNREF(a[0].ref);
    if (!s) return r;
    char* name = vmStringToUTF(t, s);
    if (!name) return r;
    size_t len = 0;
    uint8_t* p = zipReadFile(NULL, name, &len);
    if (!p && name[0] == '/') p = zipReadFile(NULL, name + 1, &len);
    free(name);
    if (!p) return r;
    ArrayObject* arr = vmNewTypeArray(t, EK_BYTE, (int32_t)len);
    if (arr && len > 0) memcpy(ARRAY_DATA(arr), p, len);
    free(p);
    if (!arr) { if (vmHasException(t)) vmClearException(t); return r; }
    r.ref = JREF((Object*)arr);
    return r;
}

void bridgeRegisterCommonNatives(void) {
    vmRegisterNative("javax/microedition/lcdui/Graphics", "rasterText",
                     "([IIIIIIIIILjava/lang/String;I)V", n_rasterText);
    vmRegisterNative("javax/microedition/lcdui/Image", "bridgeReadResource",
                     "(Ljava/lang/String;)[B", n_bridgeReadResource);
    vmRegisterNative("javax/microedition/lcdui/Image", "decodePng",
                     "([B)Ljavax/microedition/lcdui/Image;", n_decodePng);
    vmRegisterNative("javax/microedition/rms/RecordStore", "nativeLoadStore",
                     "(Ljava/lang/String;)[B", n_loadStore);
    vmRegisterNative("javax/microedition/rms/RecordStore", "nativeSaveStore",
                     "(Ljava/lang/String;[B)V", n_saveStore);
    vmRegisterNative("javax/microedition/rms/RecordStore", "nativeListStores",
                     "()[Ljava/lang/String;", n_listStores);
    vmRegisterNative("javax/microedition/rms/RecordStore", "nativeDeleteStore",
                     "(Ljava/lang/String;)Z", n_deleteStore);
    vmRegisterNative("java/lang/Class", "nativeGetResource",
                     "(Ljava/lang/String;)[B", n_classGetResource);
    /* MMAPI */
    vmRegisterNative("javax/microedition/media/Manager", "nPlayTone",
                     "(III)I", n_playTone);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nCreate",
                     "([BLjava/lang/String;)I", n_mediaCreate);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nCreateToneSeq",
                     "([B)I", n_mediaCreateToneSeq);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nStart",
                     "(I)I", n_mediaStart);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nStop",
                     "(I)I", n_mediaStop);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nClose",
                     "(I)V", n_mediaClose);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nPrefetch",
                     "(I)I", n_mediaPrefetch);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nSetLoopCount",
                     "(II)V", n_mediaSetLoop);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nSetVolume",
                     "(II)V", n_mediaSetVolume);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nGetVolume",
                     "(I)I", n_mediaGetVolume);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nState",
                     "(I)I", n_mediaState);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nDuration",
                     "(I)J", n_mediaDuration);
    vmRegisterNative("javax/microedition/media/PlayerImpl", "nPosition",
                     "(I)J", n_mediaPosition);
}
