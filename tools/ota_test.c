/* ota_test.c - Nokia OTA 铃声合成单元测试（本地 Windows / CI macOS 共用）
 * 数据来源：《歪歪歪传战国风云》War/c.class 内嵌 FORMAT_TONE 音序（43 字节）
 * 期望：约 2.9s / ~64k 样本 / 有效幅度（非静音）
 * 用法：gcc -O2 -I Bridge -o ota_test tools/ota_test.c Bridge/midi_synth.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include "midi_synth.h"

static const uint8_t ww_tone[] = {
    0x00,0x4A,0x3A,0x40,0x04,0x00,0x31,0x26,0x46,0x24,0x62,0x45,
    0xA4,0x20,0x83,0x0D,0x21,0x04,0x95,0x89,0x58,0x95,0x69,0x08,
    0x11,0x69,0x08,0x11,0x89,0x58,0x98,0x69,0x08,0x18,0x69,0x08,
    0x16,0x89,0x58,0x93,0x48,0x00,0x02
};

int main(void) {
    size_t n = 0;
    int16_t* pcm = synthOta(ww_tone, sizeof(ww_tone), &n);
    if (!pcm) { fprintf(stderr, "FAIL: synthOta returned NULL\n"); return 1; }
    double dur = (double)n / (double)MSYNTH_RATE;
    printf("synthOta: %zu samples, %.2fs\n", n, dur);
    if (n < 55000 || n > 70000) {
        fprintf(stderr, "FAIL: sample count out of range (expect ~64k for 2.9s)\n");
        free(pcm); return 1;
    }
    long sum = 0;
    for (size_t i = 0; i < n; i++) sum += pcm[i] < 0 ? -pcm[i] : pcm[i];
    printf("avg abs amplitude = %ld\n", sum / (long)n);
    if (sum / (long)n < 2000) {
        fprintf(stderr, "FAIL: output too quiet (silence?)\n");
        free(pcm); return 1;
    }
    free(pcm);
    printf("OTA TEST PASS\n");
    return 0;
}
