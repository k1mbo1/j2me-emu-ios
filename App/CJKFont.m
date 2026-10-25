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
    unsigned char bits[24];
} CJKEntry;

static CJKEntry g_cache[CACHE_SLOTS];
static CTFontRef g_font;
static CGFloat g_ascent;

static void cjkInit(void) {
    if (g_font) return;
    g_font = CTFontCreateWithName(CFSTR("PingFangSC-Regular"), 12.0, NULL);
    if (!g_font) g_font = CTFontCreateWithName(CFSTR("Heiti SC"), 12.0, NULL);
    if (!g_font) g_font = CTFontCreateWithName(CFSTR("Helvetica"), 12.0, NULL);
    g_ascent = g_font ? CTFontGetAscent(g_font) : 0;
}

/* 把单字渲染到 12x12 灰度位图，阈值化后写入位掩码 */
static int renderGlyph(unsigned int c, unsigned char* out24) {
    cjkInit();
    if (!g_font) return 0;

    UniChar u = (UniChar)c;
    CGGlyph glyph = 0;
    if (!CTFontGetGlyphsForCharacters(g_font, &u, &glyph, 1) || glyph == 0) return 0;

    uint8_t gray[12 * 12];
    memset(gray, 0, sizeof(gray));
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
    if (!cs) return 0;
    CGContextRef ctx = CGBitmapContextCreate(gray, 12, 12, 8, 12, cs, kCGImageAlphaNone);
    CGColorSpaceRelease(cs);
    if (!ctx) return 0;

    CGContextSetGrayFillColor(ctx, 1.0, 1.0);
    CGContextSetShouldAntialias(ctx, true);
    CGContextSetShouldSmoothFonts(ctx, false);
    /* CG 绘制坐标原点在左下：基线离底部 (12 - ascent)，让 12px 字身垂直居中 */
    CGFloat base = (g_ascent > 8.0 && g_ascent < 12.0) ? (12.0 - g_ascent) : 3.0;
    CGPoint pos = CGPointMake(0.5f, (float)base);
    CTFontDrawGlyphs(g_font, &glyph, &pos, 1, ctx);
    CGContextRelease(ctx);

    memset(out24, 0, 24);
    /* CGBitmapContext 内存首行即顶部（top-down），与点阵约定（顶行在前、
     * 高位在左，同 Windows GDI 端）一致：直接映射，不再翻转 */
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < 12; x++) {
            if (gray[y * 12 + x] < 90) continue;
            out24[y * 2 + (x >> 3)] |= (unsigned char)(0x80 >> (x & 7));
        }
    }
    return 1;
}

int emuCjkGlyph(unsigned int c, unsigned char* out24) {
    if (c < 0x2E80 || c > 0xFFFF) return 0;
    int slot = (int)((c * 2654435761u) % CACHE_SLOTS);
    for (int k = 0; k < 8; k++) {
        int idx = (slot + k) & (CACHE_SLOTS - 1);
        if (g_cache[idx].code == c) {
            memcpy(out24, g_cache[idx].bits, 24);
            return 1;
        }
        if (g_cache[idx].code == 0) {
            unsigned char bits[24];
            if (!renderGlyph(c, bits)) return 0;
            g_cache[idx].code = c;
            memcpy(g_cache[idx].bits, bits, 24);
            memcpy(out24, bits, 24);
            return 1;
        }
    }
    /* 缓存满：直接渲染不缓存 */
    return renderGlyph(c, out24);
}
