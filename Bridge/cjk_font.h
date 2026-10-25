#ifndef CJK_FONT_H
#define CJK_FONT_H

/*
 * 平台字形回调：把宽字符（中文/全角等）栅格化为 12x12 灰度覆盖率。
 * out 为 144 字节（12 行 x 12 列，逐字节 0-255 覆盖率，行 0 = 顶部）。
 * 保留抗锯齿灰度可让笔画边缘平滑（二值化会丢边缘导致发虚）。
 * 返回 1 表示成功（调用方按 12 像素步进），0 表示无字形（调用方按 '?' 8 像素处理）。
 * 由各平台实现：iOS = App/CJKFont.m（CoreText）；Windows = tools/cjk_font_win.c（GDI）。
 */
int emuCjkGlyph(unsigned int c, unsigned char* out144);

/* 宽字符判定（与 MIDP Font 的宽度规则保持一致） */
static inline int emuIsWideChar(unsigned int c) {
    return c >= 0x2E80;
}

#endif
