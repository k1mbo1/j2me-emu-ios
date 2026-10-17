/* bridge_common.h - 平台无关 Bridge natives（Windows headless / iOS 共用） */
#ifndef BRIDGE_COMMON_H
#define BRIDGE_COMMON_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RMS 持久化目录（headless 设本地目录；iOS 设 Documents 子目录） */
void bridgeSetRmsDir(const char* dir);

/* 注册共用 natives：
 *   Graphics.rasterText / Image.bridgeReadResource / Image.decodePng
 *   RecordStore.nativeLoadStore / nativeSaveStore / nativeListStores
 * 平台相关 natives（repaint/vibrate/http/midlet 生命周期）由各端自行注册 */
void bridgeRegisterCommonNatives(void);

#ifdef __cplusplus
}
#endif

#endif
