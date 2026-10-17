package com.nokia.mid.ui;

/* DirectGraphics（Nokia UI API）：扩展绘图接口（旋转/翻转/像素级操作）。
 * 兼容实现：接口完整保留，画面操作按非变换语义执行。 */
public interface DirectGraphics {

    int ROTATE_90       = 16384;   /* 2^14 */
    int ROTATE_180      = 32768;   /* 2^15 */
    int ROTATE_270      = 65536;   /* 2^16 */
    int MIRROR          = 131072;  /* 2^17 */
    int MIRROR_ROT180   = 262144;  /* 2^18 */

    int TYPE_BYTE_1_GRAY    = -1;
    int TYPE_BYTE_2_GRAY    = -2;
    int TYPE_BYTE_4_GRAY    = -3;
    int TYPE_BYTE_8_GRAY    = -4;
    int TYPE_BYTE_332_RGB   = -5;
    int TYPE_USHORT_444_RGB = -6;
    int TYPE_USHORT_555_RGB = -7;
    int TYPE_USHORT_565_RGB = -10;
    int TYPE_INT_888_RGB    = -11;
    int TYPE_INT_8888_ARGB  = -12;

    void drawImage(javax.microedition.lcdui.Image img, int x, int y, int anchor, int manipulation);
    void drawPixels(int[] pixels, boolean transparency, int offset, int scanlength,
                    int x, int y, int width, int height, int manipulation, int format);
    void drawPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
                    int x, int y, int width, int height, int manipulation, int format);
    void drawPolygon(int[] xPoints, int[] yPoints, int nPoints, int manipulation);
    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int manipulation);
    void fillPolygon(int[] xPoints, int[] yPoints, int nPoints, int manipulation);
    void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int manipulation);
    void getPixels(int[] pixels, int offset, int scanlength,
                   int x, int y, int width, int height, int format);
    void getPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
                   int x, int y, int width, int height, int format);
    int getAlphaComponent();
    int getNativePixelFormat();
}
