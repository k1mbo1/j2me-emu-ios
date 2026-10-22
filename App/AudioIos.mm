/* AudioIos.mm - AVAudioPlayer 池实现
 * - ambient session：与系统音乐混音、静音开关生效
 * - autoClose 句柄（playTone 一次性音效）：播完自动回收
 * - PauseAll/ResumeAll 配合暂停菜单；StopAll 退出游戏
 */
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import "AudioIos.h"

@interface J2AudioDelegate : NSObject <AVAudioPlayerDelegate>
@end

static NSMutableDictionary* gPlayers = nil;   /* NSNumber -> AVAudioPlayer */
static NSMutableSet* gAutoClose = nil;
static NSMutableSet* gPaused = nil;           /* PauseAll 暂停的句柄（ResumeAll 恢复） */
static NSMutableDictionary* gVol = nil;       /* NSNumber -> NSNumber 游戏设置的音量(0-100) */
static int gMaster = 100;                     /* 全局主音量系数 */
static J2AudioDelegate* gDelegate = nil;
static int gNextHandle = 1;
static BOOL gSessionConfigured = NO;

@implementation J2AudioDelegate
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer*)player successfully:(BOOL)flag {
    (void)flag;
    NSNumber* key = nil;
    for (NSNumber* k in gPlayers) {
        if (gPlayers[k] == player) { key = k; break; }
    }
    if (key && [gAutoClose containsObject:key]) {
        [gPlayers removeObjectForKey:key];
        [gAutoClose removeObject:key];
        [gVol removeObjectForKey:key];
    }
}
@end

static void ensureInit(void) {
    if (gPlayers) return;
    gPlayers = [NSMutableDictionary new];
    gAutoClose = [NSMutableSet new];
    gPaused = [NSMutableSet new];
    gVol = [NSMutableDictionary new];
    gDelegate = [J2AudioDelegate new];
}

/* 实际音量 = 游戏设置音量 x 主音量 */
static void applyVolume(int h, AVAudioPlayer* p) {
    int gameVol = [gVol[@(h)] intValue];
    if (gameVol <= 0) gameVol = 100;
    int m = gMaster;
    if (m < 0) m = 0;
    if (m > 100) m = 100;
    p.volume = (float)gameVol / 100.0f * (float)m / 100.0f;
}

static void ensureSession(void) {
    if (gSessionConfigured) return;
    gSessionConfigured = YES;
    NSError* err = nil;
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient
                                            mode:AVAudioSessionModeDefault
                                         options:0
                                           error:&err];
    if (err) NSLog(@"[audio] session category error: %@", err);
}

static AVAudioPlayer* playerFor(int h) {
    NSNumber* k = [NSNumber numberWithInt:h];
    return gPlayers[k];
}

int audioIosCreate(const void* audioBytes, size_t len, int autoClose) {
    if (!audioBytes || len == 0) return -1;
    ensureInit();
    ensureSession();
    NSData* data = [NSData dataWithBytes:audioBytes length:len];
    NSError* err = nil;
    AVAudioPlayer* p = [[AVAudioPlayer alloc] initWithData:data error:&err];
    if (!p || err) {
        NSLog(@"[audio] create failed: %@", err ? err.localizedDescription : @"?");
        return -1;
    }
    p.delegate = gDelegate;
    int h = gNextHandle++;
    gPlayers[@(h)] = p;
    gVol[@(h)] = @100;
    if (autoClose) [gAutoClose addObject:@(h)];
    return h;
}

int audioIosStart(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return -1;
    return [p play] ? 0 : -1;
}

int audioIosStop(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return -1;
    [p stop];
    p.currentTime = 0;
    return 0;
}

void audioIosClose(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return;
    [p stop];
    [gPlayers removeObjectForKey:@(h)];
    [gAutoClose removeObject:@(h)];
    [gVol removeObjectForKey:@(h)];
}

int audioIosPrefetch(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return -1;
    return [p prepareToPlay] ? 0 : -1;
}

void audioIosSetVolume(int h, int vol) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    gVol[@(h)] = @(vol);
    applyVolume(h, p);
}

int audioIosVolume(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return -1;
    return [gVol[@(h)] intValue];
}

int audioIosState(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return -1;
    return p.isPlaying ? 1 : 0;
}

long audioIosDurationMs(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return -1;
    NSTimeInterval d = p.duration;
    if (d <= 0) return -1;
    return (long)(d * 1000.0);
}

long audioIosPositionMs(int h) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return 0;
    return (long)(p.currentTime * 1000.0);
}

void audioIosSetLoop(int h, int count) {
    AVAudioPlayer* p = playerFor(h);
    if (!p) return;
    /* JSR: count=-1 无限; count=n 播 n 遍。AVAudioPlayer: numberOfLoops = 播放次数-1 */
    p.numberOfLoops = (count < 0) ? -1 : (count > 0 ? count - 1 : 0);
}

void audioIosPauseAll(void) {
    if (!gPlayers) return;
    [gPaused removeAllObjects];
    for (NSNumber* k in gPlayers) {
        AVAudioPlayer* p = gPlayers[k];
        if (p.isPlaying) {
            [p pause];
            [gPaused addObject:k];
        }
    }
}

void audioIosResumeAll(void) {
    if (!gPlayers) return;
    for (NSNumber* k in gPaused) {
        AVAudioPlayer* p = gPlayers[k];   /* 可能已被 autoClose 回收 */
        if (p) [p play];
    }
    [gPaused removeAllObjects];
}

void audioIosStopAll(void) {
    if (!gPlayers) return;
    NSArray* keys = [gPlayers allKeys];
    for (NSNumber* k in keys) {
        AVAudioPlayer* p = gPlayers[k];
        [p stop];
        [gPlayers removeObjectForKey:k];
    }
    [gAutoClose removeAllObjects];
    [gPaused removeAllObjects];
    [gVol removeAllObjects];
}

void audioIosSetMasterVolume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    gMaster = vol;
    if (!gPlayers) return;
    for (NSNumber* k in gPlayers) {
        AVAudioPlayer* p = gPlayers[k];
        if (p) applyVolume(k.intValue, p);
    }
}

int audioIosMasterVolume(void) {
    return gMaster;
}
