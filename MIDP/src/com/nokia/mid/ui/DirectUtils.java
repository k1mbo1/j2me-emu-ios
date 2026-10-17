package com.nokia.mid.ui;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

/* DirectUtils（Nokia UI API）。
 * getDirectGraphics 返回包装实现：drawImage 委托 Graphics 正常绘制，
 * 其余像素级/多边形操作为兼容空实现。 */
public final class DirectUtils {

    private DirectUtils() { }

    public static Image createImage(byte[] imageData, int imageOffset, int imageLength) {
        try {
            return Image.createImage(imageData, imageOffset, imageLength);
        } catch (Exception e) {
            return null;   /* Nokia 语义：解码失败不抛异常 */
        }
    }

    public static Image createImage(int width, int height) {
        return Image.createImage(width, height);
    }

    public static Image makeImmutableImage(Image img) {
        return img == null ? null : Image.createImage(img);
    }

    public static Image makeMutableImage(Image img) {
        return img == null ? null : Image.createImage(img);
    }

    public static DirectGraphics getDirectGraphics(Graphics g) {
        return new DGImpl(g);
    }

    /* 包装实现 */
    private static final class DGImpl implements DirectGraphics {
        private final Graphics g;
        DGImpl(Graphics g) { this.g = g; }

        public void drawImage(Image img, int x, int y, int anchor, int manipulation) {
            if (g != null && img != null) g.drawImage(img, x, y, anchor);
        }
        public void drawPixels(int[] pixels, boolean transparency, int offset, int scanlength,
                               int x, int y, int width, int height, int manipulation, int format) { }
        public void drawPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
                               int x, int y, int width, int height, int manipulation, int format) { }
        public void drawPolygon(int[] xPoints, int[] yPoints, int nPoints, int manipulation) { }
        public void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int manipulation) { }
        public void fillPolygon(int[] xPoints, int[] yPoints, int nPoints, int manipulation) { }
        public void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int manipulation) { }
        public void getPixels(int[] pixels, int offset, int scanlength,
                              int x, int y, int width, int height, int format) { }
        public void getPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength,
                              int x, int y, int width, int height, int format) { }
        public int getAlphaComponent() { return 0; }
        public int getNativePixelFormat() { return TYPE_INT_8888_ARGB; }
    }
}
