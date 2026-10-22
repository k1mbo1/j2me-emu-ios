/* AudioIos.h - iOS 音频平台层（AVAudioPlayer，纯 C 接口，与 vm.h 隔离）
 * Bridge MediaPlatform 的 iOS 实现。所有接口主线程调用（与 VM 同线程）。
 */
#ifndef AUDIO_IOS_H
#define AUDIO_IOS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int  audioIosCreate(const void* audioBytes, size_t len, int autoClose); /* 句柄，-1 失败 */
int  audioIosStart(int h);          /* 0 成功 */
int  audioIosStop(int h);           /* 0 成功 */
void audioIosClose(int h);
int  audioIosPrefetch(int h);       /* prepareToPlay，0 成功 */
void audioIosSetVolume(int h, int vol);   /* 0-100 */
int  audioIosVolume(int h);         /* -1 未知 */
int  audioIosState(int h);          /* 1 播放中 0 停止 -1 未知 */
long audioIosDurationMs(int h);     /* -1 未知 */
long audioIosPositionMs(int h);
void audioIosSetLoop(int h, int count);   /* -1 无限, n 播 n 遍 */
void audioIosPauseAll(void);        /* 暂停菜单 / 退后台 */
void audioIosResumeAll(void);
void audioIosStopAll(void);         /* 退出游戏 */

/* 全局主音量 0-100（所有播放器的统一系数，与单个播放器音量相乘） */
void audioIosSetMasterVolume(int vol);
int  audioIosMasterVolume(void);

#ifdef __cplusplus
}
#endif

#endif
