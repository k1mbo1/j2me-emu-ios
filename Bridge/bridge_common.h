/* bridge_common.h - 平台无关 Bridge natives（Windows headless / iOS 共用） */
#ifndef BRIDGE_COMMON_H
#define BRIDGE_COMMON_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RMS 持久化目录（headless 设本地目录；iOS 设 Documents 子目录） */
void bridgeSetRmsDir(const char* dir);

/* ============ 媒体平台挂钩（MMAPI 音效） ============ */
/* 平台实现播放：bridge 层负责 MIDI/ToneControl 软件合成 + WAV 封装，
 * 平台层只管播 WAV/MP3 字节流。headless 实现写文件验证；iOS 为 AVAudioPlayer。 */
typedef struct MediaPlatform {
    int  (*create)(const void* wavBytes, size_t wavLen, int autoClose); /* 播放器句柄，-1 失败 */
    int  (*start)(int h);          /* 0 成功 */
    int  (*stop)(int h);           /* 0 成功 */
    void (*close)(int h);
    int  (*prefetch)(int h);       /* 预解码，0 成功（可 no-op） */
    void (*setVolume)(int h, int vol);  /* 0-100 */
    int  (*volume)(int h);         /* -1 未知 */
    int  (*state)(int h);          /* 1 播放中 0 停止 -1 未知 */
    long (*durationMs)(int h);     /* -1 未知 */
    long (*positionMs)(int h);
    void (*setLoop)(int h, int count); /* count: -1 无限, n 播 n 遍 */
    void (*pauseAll)(void);
    void (*resumeAll)(void);
    void (*stopAll)(void);
} MediaPlatform;
void bridgeSetMediaPlatform(const MediaPlatform* mp);   /* NULL 清除（全部变 no-op） */

/* 注册共用 natives：
 *   Graphics.rasterText / Image.bridgeReadResource / Image.decodePng
 *   RecordStore.nativeLoadStore / nativeSaveStore / nativeListStores
 *   Class.nativeGetResource / Manager.nPlayTone / PlayerImpl.n*
 * 平台相关 natives（repaint/vibrate/http/midlet 生命周期）由各端自行注册 */
void bridgeRegisterCommonNatives(void);

#ifdef __cplusplus
}
#endif

#endif
