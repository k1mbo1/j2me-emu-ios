/* iOSBridge.h - iOS 平台 Bridge：MIDlet 运行时 + 平台 natives
 * UI 层（GameViewController 等）通过本接口驱动 VM。
 * 线程约定：所有接口必须在主线程调用（VM 无锁，与帧循环同线程）。
 */
#ifndef IOS_BRIDGE_H
#define IOS_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 启动 MIDlet：加载 game jar、解析 manifest 取主类、执行 <init> + startApp。
 * midpJarPath: 内置 API jar（NSBundle 里的 midp.jar 路径）。
 * rmsDir: 本游戏独立存档目录（调用方保证每游戏唯一）。
 * screenW/H: Canvas 逻辑分辨率（240x320）。
 * 返回 0 成功，非 0 失败（j2meLastError 取原因） */
int  j2meStart(const char* midpJarPath, const char* gameJarPath, const char* rmsDir,
               int screenW, int screenH);

/* 帧驱动：执行当前 Canvas 的 doFrame()（dirty 则 paint）。
 * 返回像素缓冲（0xAARRGGBB，仅在本帧内有效），w/h 为 0 表示当前无 Canvas。
 * 返回 NULL 表示游戏已死（异常/结束），错误经 j2meLastError 获取。 */
const uint32_t* j2meFrame(int* w, int* h);

/* 键事件。keyCode=标准 Canvas 键码（KEY_NUM0..9='0'..'9'）。
 * down: 1=按下 0=释放 2=重复 */
void j2meKey(int keyCode, int down);

/* 生命周期（暂停时 UI 层应同时停帧） */
void j2mePause(void);    /* pauseApp */
void j2meResume(void);   /* startApp */
void j2meDestroy(void);  /* destroyApp(true) + vmShutdown */

/* 状态查询 */
int         j2meIsRunning(void);
const char* j2meLastError(void);
const char* j2meMidletName(void);   /* manifest MIDlet-Name（未解析到则文件名） */

/* 不启动 VM，直接读 jar manifest 的 MIDlet-1 显示名（游戏库列表用）。
 * 成功返回 0 且 nameOut 填入显示名；失败返回非 0。 */
int  j2mePeekManifest(const char* jarPath, char* nameOut, size_t cap);

/* 全局媒体音量 0-100（MMAPI 所有播放器统一系数，游戏内设置与之相乘） */
void j2meSetVolume(int vol);
int  j2meVolume(void);

#ifdef __cplusplus
}
#endif

#endif /* IOS_BRIDGE_H */
