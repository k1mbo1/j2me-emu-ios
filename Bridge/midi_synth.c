/* midi_synth.c - 软件音源实现
 * 波形模型：0.6*正弦 + 0.4*方波，3ms attack / 45ms release 线性包络。
 * 音高：f = 440 * 2^((note-69)/12)。复音累加后限幅 ±32000。
 * MIDI 解析：MThd/MTrk、VLQ delta、running status、program change、
 *   note on/off 配对、FF 51 tempo 变更（分段线性 tick->ms）。
 * ToneControl 音序：duration 单位 = 1/64 个四分音符（d=64 -> 1 拍）。
 */
#include "midi_synth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============ 基础波形 ============ */

static double noteFreq(int note) { return 440.0 * pow(2.0, (note - 69) / 12.0); }

/* 单音符渲染到缓冲（累加）。t0/t1 为采样区间 [start, end) */
static void renderNote(int32_t* buf, size_t nSamples,
                       int note, long startMs, long lenMs, double amp,
                       int squareBias) {
    long rate = MSYNTH_RATE;
    long s0 = startMs * rate / 1000;
    long s1 = (startMs + lenMs) * rate / 1000;
    if (s0 < 0) s0 = 0;
    if (s1 > (long)nSamples) s1 = (long)nSamples;
    if (s1 <= s0) return;
    double f = noteFreq(note);
    double w = 2.0 * 3.14159265358979 * f / (double)rate;
    long atk = rate * 3 / 1000;      /* 3ms attack */
    long rel = rate * 45 / 1000;     /* 45ms release */
    long total = s1 - s0;
    for (long i = 0; i < total; i++) {
        double ph = (double)(s0 + i) * w;
        double s = sin(ph);
        double sq = s >= 0 ? 1.0 : -1.0;
        double v = (1.0 - squareBias * 0.5) * s + (squareBias * 0.5) * sq;
        double env = 1.0;
        if (i < atk) env = (double)i / (double)atk;
        if (total - i < rel) {
            double e = (double)(total - i) / (double)rel;
            if (e < env) env = e;
        }
        buf[s0 + i] += (int32_t)(v * env * amp);
    }
}

static int16_t* finalize(int32_t* acc, size_t n) {
    int16_t* out = (int16_t*)malloc(n ? n * sizeof(int16_t) : sizeof(int16_t));
    if (!out) { free(acc); return NULL; }
    for (size_t i = 0; i < n; i++) {
        int32_t v = acc[i];
        if (v > 32000) v = 32000;
        if (v < -32000) v = -32000;
        out[i] = (int16_t)v;
    }
    return out;
}

/* ============ 单音符（playTone） ============ */

int16_t* synthTone(int note, int durMs, int volume, size_t* outLen) {
    *outLen = 0;
    if (note < 0 || note > 127 || durMs <= 0 || volume < 0 || volume > 100)
        return NULL;
    if (durMs > 10000) durMs = 10000;
    size_t n = (size_t)(durMs + 60) * MSYNTH_RATE / 1000;   /* 尾部 release */
    int32_t* acc = (int32_t*)calloc(n ? n : 1, sizeof(int32_t));
    if (!acc) return NULL;
    renderNote(acc, n, note, 0, durMs, 12000.0 * volume / 100.0, 1);
    int16_t* r = finalize(acc, n);
    if (r) *outLen = n;
    return r;
}

/* ============ ToneControl 音序 ============ */

#define TC_VERSION     (-2)
#define TC_BLOCK_START (-3)
#define TC_BLOCK_END   (-4)
#define TC_PLAY_BLOCK  (-5)
#define TC_SET_VOLUME  (-6)
#define TC_REPEAT      (-7)
#define TC_SILENCE     (-95)

int16_t* synthToneSeq(const uint8_t* seq, size_t len, size_t* outLen) {
    *outLen = 0;
    if (!seq || len < 2) return NULL;
    const int8_t* s = (const int8_t*)seq;
    size_t i = 0;
    int tempo = 120, resolution = 64;
    if (s[0] == TC_VERSION) { tempo = s[1]; i = 2; }
    if (i + 1 < len && s[i] == TC_VERSION) { resolution = s[i + 1]; i += 2; }
    if (tempo < 5) tempo = 5;
    if (tempo > 300) tempo = 300;
    if (resolution < 1 || resolution > 128) resolution = 64;
    /* duration d 的时长 ms = d * (60000 / tempo) / 64 */
    double msPerUnit = 60000.0 / (double)tempo / 64.0;

    /* 收集事件后再分配：先估算总时长上限（len*64 单位过于宽松，用两遍法） */
    typedef struct { int note; long startMs; long durMs; int vol; } Ev;
    Ev* evs = (Ev*)malloc((len + 1) * sizeof(Ev));
    if (!evs) return NULL;
    int nEv = 0;
    long cursorMs = 0;
    int volume = 100;

    /* 块缓存：最多 128 块，每块记录起止下标 */
    int blkStart[128], blkLen[128], nBlk = 0;
    size_t pos = i;
    int guard = 0;
    while (pos + 1 < len && guard++ < 100000) {
        int note = s[pos], d = s[pos + 1];
        pos += 2;
        if (note == TC_VERSION) { tempo = d < 5 ? 5 : d; msPerUnit = 60000.0 / (double)tempo / 64.0; continue; }
        if (note == TC_SET_VOLUME) { volume = d < 0 ? 0 : (d > 100 ? 100 : d); continue; }
        if (note == TC_BLOCK_START) {
            if (d >= 0 && d < 128) { if (nBlk < 128) { blkStart[nBlk] = nEv; blkLen[nBlk] = -1; nBlk++; } }
            else if (d >= 128 && nBlk > 0) { /* BLOCK_END 用负数 d 表示块号负值 */ }
            continue;
        }
        if (note == TC_BLOCK_END) {
            /* d 为负块号或正值块号-128？规范：BLOCK_END 后跟块号（负数）。
             * 我们在写入时把 BLOCK_END 也写 blkStart 标记法：直接关最近块 */
            if (nBlk > 0 && blkLen[nBlk - 1] == -1) blkLen[nBlk - 1] = nEv - blkStart[nBlk - 1];
            continue;
        }
        if (note == TC_PLAY_BLOCK) {
            int b = d < 0 ? -d : d;
            if (b >= 0 && b < nBlk && blkLen[b] > 0) {
                for (int k = 0; k < blkLen[b]; k++) {
                    Ev* e = &evs[blkStart[b] + k];
                    evs[nEv].note = e->note;
                    evs[nEv].durMs = e->durMs;
                    evs[nEv].vol = e->vol;
                    evs[nEv].startMs = cursorMs;
                    cursorMs += e->durMs;
                    nEv++;
                }
            }
            continue;
        }
        if (note == TC_REPEAT) {
            int cnt = d < 0 ? 2 : d;
            /* 重复上一个音符/段：简化为重复前一音符 cnt 次 */
            if (nEv > 0) {
                Ev* e = &evs[nEv - 1];
                for (int k = 1; k < cnt; k++) {
                    evs[nEv].note = e->note;
                    evs[nEv].durMs = e->durMs;
                    evs[nEv].vol = e->vol;
                    evs[nEv].startMs = cursorMs;
                    cursorMs += e->durMs;
                    nEv++;
                }
            }
            continue;
        }
        /* 音符 / 休止 */
        int isSilence = (note == TC_SILENCE);
        long durMs = (long)((double)d * msPerUnit);
        if (durMs < 10) durMs = 10;
        if (isSilence) {
            evs[nEv].note = -1;
            evs[nEv].durMs = durMs;
            evs[nEv].vol = 0;
            evs[nEv].startMs = cursorMs;
        } else {
            evs[nEv].note = note < 0 ? 0 : (note > 127 ? 127 : note);
            evs[nEv].durMs = durMs;
            evs[nEv].vol = volume;
            evs[nEv].startMs = cursorMs;
        }
        cursorMs += durMs;
        nEv++;
    }
    if (nEv == 0) { free(evs); return NULL; }

    size_t n = (size_t)(cursorMs + 100) * MSYNTH_RATE / 1000;
    if (n > 22050UL * 600UL) n = 22050UL * 600UL;   /* 最长 10 分钟 */
    int32_t* acc = (int32_t*)calloc(n ? n : 1, sizeof(int32_t));
    if (!acc) { free(evs); return NULL; }
    for (int k = 0; k < nEv; k++) {
        if (evs[k].note < 0) continue;
        renderNote(acc, n, evs[k].note, evs[k].startMs, evs[k].durMs,
                   12000.0 * evs[k].vol / 100.0, 1);
    }
    free(evs);
    int16_t* r = finalize(acc, n);
    if (r) *outLen = n;
    return r;
}

/* ============ SMF MIDI 合成 ============ */

typedef struct { long tick; int type, ch, d1, d2; } MEv;   /* type: 0=noteoff 1=noteon 2=program */
typedef struct { long tick; int usecPerQn; } TempoEv;

static int rdU32(const uint8_t* p) {
    return ((int)p[0] << 24) | ((int)p[1] << 16) | ((int)p[2] << 8) | p[3];
}

int16_t* synthMidi(const uint8_t* mid, size_t len, size_t* outLen) {
    *outLen = 0;
    if (!mid || len < 14) return NULL;
    if (memcmp(mid, "MThd", 4) != 0) return NULL;
    size_t hdLen = (size_t)rdU32(mid + 4);
    int division = (int)((mid[12] << 8) | mid[13]);
    if (division <= 0) return NULL;    /* SMPTE 格式不支持（极罕见） */

    MEv* evs = NULL; int nEv = 0, capEv = 0;
    TempoEv* tempos = NULL; int nT = 0, capT = 0;
    long maxTick = 0;

    size_t off = 8 + hdLen;
    int nTracks = 0;
    while (off + 8 <= len && nTracks < 64) {
        if (memcmp(mid + off, "MTrk", 4) != 0) { off += 4; continue; }
        size_t trkLen = (size_t)rdU32(mid + off + 4);
        const uint8_t* p = mid + off + 8;
        const uint8_t* end = p + trkLen;
        if ((size_t)(end - mid) > len) end = mid + len;
        long tick = 0;
        int runStatus = 0;
        int prog[16] = {0};
        while (p < end) {
            /* VLQ delta */
            long d = 0;
            int b;
            do { b = *p++; if (p > end) break; d = (d << 7) | (b & 0x7F); } while (b & 0x80);
            tick += d;
            if (p >= end) break;
            int st = *p;
            if (st & 0x80) { p++; runStatus = st; } else { st = runStatus; if (!st) break; }
            int hi = st & 0xF0, ch = st & 0x0F;
            if (hi == 0x90 || hi == 0x80) {
                if (p + 1 >= end) break;
                int note = p[0], vel = p[1]; p += 2;
                if (hi == 0x90 && vel > 0) {
                    if (nEv == capEv) { capEv = capEv ? capEv * 2 : 256; evs = (MEv*)realloc(evs, capEv * sizeof(MEv)); if (!evs) goto fail; }
                    evs[nEv].tick = tick; evs[nEv].type = 1; evs[nEv].ch = ch; evs[nEv].d1 = note; evs[nEv].d2 = vel; nEv++;
                } else {
                    if (nEv == capEv) { capEv = capEv ? capEv * 2 : 256; evs = (MEv*)realloc(evs, capEv * sizeof(MEv)); if (!evs) goto fail; }
                    evs[nEv].tick = tick; evs[nEv].type = 0; evs[nEv].ch = ch; evs[nEv].d1 = note; evs[nEv].d2 = 0; nEv++;
                }
                if (tick > maxTick) maxTick = tick;
            } else if (hi == 0xC0) {
                if (p >= end) break;
                if (nEv == capEv) { capEv = capEv ? capEv * 2 : 256; evs = (MEv*)realloc(evs, capEv * sizeof(MEv)); if (!evs) goto fail; }
                evs[nEv].tick = tick; evs[nEv].type = 2; evs[nEv].ch = ch; evs[nEv].d1 = p[0]; evs[nEv].d2 = 0; nEv++;
                p += 1;
            } else if (hi == 0xD0 || hi == 0xA0 || hi == 0xB0 || hi == 0xE0) {
                p += 2;   /* 双数据字节事件忽略 */
            } else if (st == 0xFF) {
                if (p >= end) break;
                int mt = *p++;
                long ml = 0;
                do { b = *p++; if (p > end) break; ml = (ml << 7) | (b & 0x7F); } while (b & 0x80);
                if (p + ml > end) break;
                if (mt == 0x51 && ml == 3) {
                    int usec = (p[0] << 16) | (p[1] << 8) | p[2];
                    if (nT == capT) { capT = capT ? capT * 2 : 16; tempos = (TempoEv*)realloc(tempos, capT * sizeof(TempoEv)); if (!tempos) goto fail; }
                    tempos[nT].tick = tick; tempos[nT].usecPerQn = usec ? usec : 500000; nT++;
                }
                p += ml;
            } else if (st == 0xF0 || st == 0xF7) {
                long ml = 0;
                do { b = *p++; if (p > end) break; ml = (ml << 7) | (b & 0x7F); } while (b & 0x80);
                p += ml;
            } else {
                break;
            }
        }
        nTracks++;
        off += 8 + trkLen;
    }
    if (nEv == 0) goto fail;

    /* tick -> ms 分段线性（无 tempo 则 500000us/qn） */
    if (nT == 0) {
        tempos = (TempoEv*)malloc(sizeof(TempoEv));
        if (!tempos) goto fail;
        tempos[0].tick = 0; tempos[0].usecPerQn = 500000; nT = 1;
    }
    long lastMs = 0; long lastTick = 0; int curT = 0;
    long lastEventMs = 0;
    /* 配对 note on/off */
    typedef struct { int note; long startMs; int prog; int vel; } Open;
    Open open[64]; int nOpen = 0;
    typedef struct { int note, prog, vel; long sMs, eMs; } Note;
    Note* notes = (Note*)malloc((size_t)nEv * sizeof(Note));
    int* progCh = (int*)calloc(16, sizeof(int));
    if (!notes || !progCh) { free(notes); free(progCh); goto fail; }
    int nNotes = 0;

    /* tick->ms 函数（事件 tick 单调不减） */
    #define TICK2MS(tk) tickToMs((tk), tempos, nT, division)
    for (int i = 0; i < nEv; i++) {
        MEv* e = &evs[i];
        /* 推进 tempo 表 */
        long ms;
        {
            long tk = e->tick, acc = lastMs;
            long baseTk = lastTick;
            int t = curT;
            while (t + 1 < nT && tempos[t + 1].tick <= tk) t++;
            /* 分段累计 */
            acc = 0; baseTk = 0; t = 0;
            while (t + 1 < nT && tempos[t + 1].tick <= tk) {
                acc += (tempos[t + 1].tick - baseTk) * tempos[t].usecPerQn / 1000 / division;
                baseTk = tempos[t + 1].tick;
                t++;
            }
            acc += (tk - baseTk) * tempos[t].usecPerQn / 1000 / division;
            ms = acc;
        }
        if (ms > lastEventMs) lastEventMs = ms;
        if (e->type == 2) { progCh[e->ch] = e->d1; continue; }
        if (e->type == 1) {
            if (nOpen < 64) {
                open[nOpen].note = e->d1; open[nOpen].startMs = ms;
                open[nOpen].prog = progCh[e->ch]; open[nOpen].vel = e->d2;
                nOpen++;
            }
        } else {
            for (int k = 0; k < nOpen; k++) {
                if (open[k].note == e->d1) {
                    if (ms > open[k].startMs) {
                        notes[nNotes].note = open[k].note;
                        notes[nNotes].prog = open[k].prog;
                        notes[nNotes].vel = open[k].vel;
                        notes[nNotes].sMs = open[k].startMs;
                        notes[nNotes].eMs = ms;
                        nNotes++;
                    }
                    open[k] = open[nOpen - 1]; nOpen--;
                    break;
                }
            }
        }
    }
    /* 未关闭音符给 250ms */
    for (int k = 0; k < nOpen; k++) {
        notes[nNotes].note = open[k].note;
        notes[nNotes].prog = open[k].prog;
        notes[nNotes].vel = open[k].vel;
        notes[nNotes].sMs = open[k].startMs;
        notes[nNotes].eMs = open[k].startMs + 250;
        nNotes++;
    }
    if (nNotes == 0) { free(notes); free(progCh); goto fail; }

    long endMs = 0;
    for (int k = 0; k < nNotes; k++) if (notes[k].eMs > endMs) endMs = notes[k].eMs;
    endMs += 300;   /* 尾音 */
    if (endMs > 600000) endMs = 600000;

    size_t n = (size_t)endMs * MSYNTH_RATE / 1000;
    int32_t* acc = (int32_t*)calloc(n ? n : 1, sizeof(int32_t));
    if (!acc) { free(notes); free(progCh); goto fail; }
    for (int k = 0; k < nNotes; k++) {
        int sq = (notes[k].prog >= 24 && notes[k].prog < 40) ? 2 : 1;  /* 吉他类更"方" */
        double amp = 10000.0 * (notes[k].vel / 127.0);
        renderNote(acc, n, notes[k].note, notes[k].sMs,
                   notes[k].eMs - notes[k].sMs, amp, sq);
    }
    free(notes); free(progCh);
    free(evs); free(tempos);
    int16_t* r = finalize(acc, n);
    if (r) *outLen = n;
    return r;

fail:
    free(evs); free(tempos);
    return NULL;
}

/* ============ WAV 封装 ============ */

static void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void wr16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

uint8_t* pcmToWav(const int16_t* pcm, size_t nSamples, size_t* outLen) {
    *outLen = 0;
    if (!pcm || nSamples == 0) return NULL;
    size_t dataLen = nSamples * 2;
    size_t total = 44 + dataLen;
    uint8_t* w = (uint8_t*)malloc(total);
    if (!w) return NULL;
    memcpy(w, "RIFF", 4);
    wr32(w + 4, (uint32_t)(total - 8));
    memcpy(w + 8, "WAVE", 4);
    memcpy(w + 12, "fmt ", 4);
    wr32(w + 16, 16);
    wr16(w + 20, 1);            /* PCM */
    wr16(w + 22, 1);            /* mono */
    wr32(w + 24, (uint32_t)MSYNTH_RATE);
    wr32(w + 28, (uint32_t)(MSYNTH_RATE * 2));
    wr16(w + 32, 2);
    wr16(w + 34, 16);
    memcpy(w + 36, "data", 4);
    wr32(w + 40, (uint32_t)dataLen);
    memcpy(w + 44, pcm, dataLen);
    *outLen = total;
    return w;
}
