/* midi_synth.h - 软件音源（纯 C，headless/iOS 共用）
 * 1. synthTone:    MMAPI Manager.playTone 单音符
 * 2. synthToneSeq: JSR-135 ToneControl 音序（VERSION/REPEAT/BLOCK/SET_VOLUME/SILENCE）
 * 3. synthMidi:    标准 MIDI 文件（SMF format 0/1）合成
 * 4. pcmToWav:     16bit mono PCM -> WAV 字节流（iOS 交 AVAudioPlayer 播；headless 落盘验证）
 */
#ifndef MIDI_SYNTH_H
#define MIDI_SYNTH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSYNTH_RATE 22050   /* 采样率：老游戏单声道足够，省内存 */

int16_t* synthTone(int note, int durMs, int volume, size_t* outLen);
int16_t* synthToneSeq(const uint8_t* seq, size_t len, size_t* outLen);
int16_t* synthMidi(const uint8_t* mid, size_t len, size_t* outLen);
uint8_t* pcmToWav(const int16_t* pcm, size_t nSamples, size_t* outLen);

#ifdef __cplusplus
}
#endif

#endif
