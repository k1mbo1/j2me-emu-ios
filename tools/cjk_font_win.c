/* cjk_font_win.c - Windows 平台宽字符字形（GDI GetGlyphOutlineW）
 * 仅用于 headless 回归验证与 PC 版；iOS 版由 App/CJKFont.m（CoreText）实现。
 * 输出 12x12 单色点阵，24 字节（每行 2 字节，高位在左）。
 */
#ifdef _WIN32
#include <windows.h>
#include <string.h>
#include "../Bridge/cjk_font.h"

static HDC g_dc;
static HFONT g_font;
static int g_ready;

static void cjkInit(void) {
    if (g_ready) return;
    g_ready = 1;
    g_dc = CreateCompatibleDC(NULL);
    if (!g_dc) return;
    /* 12 像素高的宋体：中文 J2ME 游戏的标准点阵尺寸 */
    g_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
    if (g_font) SelectObject(g_dc, g_font);
}

int emuCjkGlyph(unsigned int c, unsigned char* out24) {
    if (c < 0x2E80 || c > 0xFFFF) return 0;
    cjkInit();
    if (!g_dc) return 0;
    memset(out24, 0, 24);
    GLYPHMETRICS gm;
    MAT2 mat;
    memset(&mat, 0, sizeof(mat));
    mat.eM11.value = 1; mat.eM22.value = 1;
    DWORD sz = GetGlyphOutlineW(g_dc, (UINT)c, GGO_BITMAP, &gm, 0, NULL, &mat);
    if (sz == GDI_ERROR || sz == 0) return 0;
    BYTE* buf = (BYTE*)malloc(sz);
    if (!buf) return 0;
    sz = GetGlyphOutlineW(g_dc, (UINT)c, GGO_BITMAP, &gm, sz, buf, &mat);
    if (sz == GDI_ERROR) { free(buf); return 0; }
    int gw = (int)gm.gmBlackBoxX, gh = (int)gm.gmBlackBoxY;
    int strideBytes = ((gw + 31) / 32) * 4;   /* GGO_BITMAP 行按 4 字节对齐 */
    int ox = (int)gm.gmptGlyphOrigin.x;
    int oy = (int)(12 - gm.gmptGlyphOrigin.y); /* 12px 字体的基线约在 y=10~11 */
    for (int row = 0; row < gh; row++) {
        int dy = oy + row;
        if (dy < 0 || dy >= 12) continue;
        const BYTE* line = buf + row * strideBytes;
        for (int col = 0; col < gw; col++) {
            if (!(line[col >> 3] & (0x80 >> (col & 7)))) continue;
            int dx = ox + col;
            if (dx < 0 || dx >= 12) continue;
            out24[dy * 2 + (dx >> 3)] |= (unsigned char)(0x80 >> (dx & 7));
        }
    }
    free(buf);
    return 1;
}

#endif
