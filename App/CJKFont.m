/*
 * CJKFont.m - iOS 平台宽字符字形（CoreText 实时栅格化 + 缓存）
 * 供 C 端 rasterText 调用：emuCjkGlyph() 输出 12x12 单色点阵。
 * 不依赖随包字体文件，使用系统字体（PingFang SC / Heiti SC）。
 */
#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>
#include <string.h>
#include "cjk_font.h"

#define CACHE_SLOTS 2048

typedef struct {
    unsigned int code;              /* 0 = 空槽 */
    unsigned char gray[144];
} CJKEntry;

static CJKEntry g_cache[CACHE_SLOTS];
static CTFontRef g_font;
static CGFloat g_ascent;

static void cjkInit(void) {
    if (g_font) return;
    /* Medium 字重：12px 小字笔画核心覆盖率高，放大后饱满清晰
     * （Regular 在小字号笔画偏细，观感发虚） */
    g_font = CTFontCreateWithName(CFSTR("PingFangSC-Medium"), 12.0, NULL);
    if (!g_font) g_font = CTFontCreateWithName(CFSTR("PingFangSC-Regular"), 12.0, NULL);
    if (!g_font) g_font = CTFontCreateWithName(CFSTR("Heiti SC"), 12.0, NULL);
    if (!g_font) g_font = CTFontCreateWithName(CFSTR("Helvetica"), 12.0, NULL);
    g_ascent = g_font ? CTFontGetAscent(g_font) : 0;
}

/* 把单字渲染到 12x12 覆盖率图（0-255）。
 * 2x 超采样（24px 渲染 -> 2x2 盒式降采样）：覆盖率估计更准，
 * 下游二值化后笔画连续不断裂（直接 12px 渲染的细笔画覆盖率
 * 分散，二值化易断笔）。 */
#define CJK_SS 2

static int renderGlyph(unsigned int c, unsigned char* out144) {
    cjkInit();
    if (!g_font) return 0;

    UniChar u = (UniChar)c;
    CGGlyph glyph = 0;
    if (!CTFontGetGlyphsForCharacters(g_font, &u, &glyph, 1) || glyph == 0) return 0;

    const int bigW = 12 * CJK_SS;
    unsigned char big[bigW * bigW];
    memset(big, 0, sizeof(big));
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
    if (!cs) return 0;
    CGContextRef ctx = CGBitmapContextCreate(big, bigW, bigW, 8, bigW, cs, kCGImageAlphaNone);
    CGColorSpaceRelease(cs);
    if (!ctx) return 0;

    CGContextSetGrayFillColor(ctx, 1.0, 1.0);
    CGContextSetShouldAntialias(ctx, true);
    CGContextScaleCTM(ctx, (CGFloat)CJK_SS, (CGFloat)CJK_SS);
    /* CG 绘制坐标原点在左下：基线离底部 (12 - ascent)，让 12px 字身垂直居中 */
    CGFloat base = (g_ascent > 8.0 && g_ascent < 12.0) ? (12.0 - g_ascent) : 3.0;
    CGPoint pos = CGPointMake(0.5f, (float)base);
    CTFontDrawGlyphs(g_font, &glyph, &pos, 1, ctx);
    CGContextRelease(ctx);

    /* 2x2 盒式降采样 -> 12x12 覆盖率（首行即顶部，top-down） */
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 12; col++) {
            int sum = 0;
            for (int sy = 0; sy < CJK_SS; sy++) {
                const unsigned char* line = big + (row * CJK_SS + sy) * bigW + col * CJK_SS;
                for (int sx = 0; sx < CJK_SS; sx++) sum += line[sx];
            }
            out144[row * 12 + col] = (unsigned char)(sum / (CJK_SS * CJK_SS));
        }
    }
    return 1;
}

int emuCjkGlyph(unsigned int c, unsigned char* out144) {
    if (c < 0x2E80 || c > 0xFFFF) return 0;
    int slot = (int)((c * 2654435761u) % CACHE_SLOTS);
    for (int k = 0; k < 8; k++) {
        int idx = (slot + k) & (CACHE_SLOTS - 1);
        if (g_cache[idx].code == c) {
            memcpy(out144, g_cache[idx].gray, 144);
            return 1;
        }
        if (g_cache[idx].code == 0) {
            unsigned char gray[144];
            if (!renderGlyph(c, gray)) return 0;
            g_cache[idx].code = c;
            memcpy(g_cache[idx].gray, gray, 144);
            memcpy(out144, gray, 144);
            return 1;
        }
    }
    /* 缓存满：直接渲染不缓存 */
    return renderGlyph(c, out144);
}
