package javax.microedition.lcdui;

import javax.microedition.lcdui.game.Sprite;

/**
 * 纯 Java 软件光栅 Graphics（ARGB int[] 目标）。
 * 文本光栅走 native rasterText（C 端 8x12 点阵，Windows/iOS 共用）。
 * translate 累加；clip 为绝对坐标交集。
 */
public class Graphics {

    public static final int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8,
            TOP = 16, BOTTOM = 32, BASELINE = 64;
    public static final int SOLID = 0, DOTTED = 1;

    /* 目标 */
    int[] data;
    int stride;

    /* 裁剪（绝对坐标） */
    int clipX, clipY, clipW, clipH;

    /* 平移（累加） */
    int transX, transY;

    int color = 0x000000;
    Font font;

    /* 供 Canvas/Image 创建（包内） */
    Graphics(int[] data, int stride, int w, int h) {
        this.data = data;
        this.stride = stride;
        clipX = 0; clipY = 0; clipW = w; clipH = h;
        font = Font.getDefaultFont();
    }

    /* ---- 颜色 ---- */
    public void setColor(int rgb) { color = rgb & 0xFFFFFF; }
    public void setColor(int r, int g, int b) {
        color = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    }
    public int getColor() { return color; }
    public int getRedComponent() { return (color >> 16) & 0xFF; }
    public int getGreenComponent() { return (color >> 8) & 0xFF; }
    public int getBlueComponent() { return color & 0xFF; }
    public int getDisplayColor(int c) { return c; }
    public void setGrayScale(int v) { setColor(v, v, v); }
    public int getGrayScale() { return (getRedComponent() * 30 + getGreenComponent() * 59 + getBlueComponent() * 11) / 100; }

    /* ---- 字体 ---- */
    public void setFont(Font f) { font = (f == null) ? Font.getDefaultFont() : f; }
    public Font getFont() { return font; }

    /* ---- 裁剪 ---- */
    public void clipRect(int x, int y, int w, int h) {
        int nx1 = Math.max(clipX, transX + x);
        int ny1 = Math.max(clipY, transY + y);
        int nx2 = Math.min(clipX + clipW, transX + x + w);
        int ny2 = Math.min(clipY + clipH, transY + y + h);
        clipX = nx1; clipY = ny1;
        clipW = nx2 > nx1 ? nx2 - nx1 : 0;
        clipH = ny2 > ny1 ? ny2 - ny1 : 0;
    }
    public void setClip(int x, int y, int w, int h) {
        int ax = transX + x, ay = transY + y;
        int ax2 = Math.max(0, ax), ay2 = Math.max(0, ay);
        int bx2 = Math.min(stride, ax + Math.max(w, 0));
        int by2 = Math.min(data.length / stride, ay + Math.max(h, 0));
        clipX = ax2; clipY = ay2;
        clipW = bx2 > ax2 ? bx2 - ax2 : 0;
        clipH = by2 > ay2 ? by2 - ay2 : 0;
    }
    public int getClipX() { return clipX - transX; }
    public int getClipY() { return clipY - transY; }
    public int getClipWidth() { return clipW; }
    public int getClipHeight() { return clipH; }

    /* ---- 平移 ---- */
    public void translate(int x, int y) { transX += x; transY += y; }
    public int getTranslateX() { return transX; }
    public int getTranslateY() { return transY; }

    /* ---- 像素原语 ---- */
    private final void px(int x, int y) {
        if (x >= clipX && x < clipX + clipW && y >= clipY && y < clipY + clipH)
            data[y * stride + x] = 0xFF000000 | color;
    }

    private final void pxSafe(int x, int y) {
        if (x >= 0 && x < stride && y >= 0 && y < data.length / stride
                && x >= clipX && x < clipX + clipW && y >= clipY && y < clipY + clipH)
            data[y * stride + x] = 0xFF000000 | color;
    }

    /* ---- 线 ---- */
    public void drawLine(int x1, int y1, int x2, int y2) {
        x1 += transX; y1 += transY; x2 += transX; y2 += transY;
        int dx = x2 - x1, dy = y2 - y1;
        int sx = dx >= 0 ? 1 : -1, sy = dy >= 0 ? 1 : -1;
        dx = Math.abs(dx); dy = Math.abs(dy);
        if (dx >= dy) {
            int y = y1, err = dy - dx / 2;
            for (int i = 0; i <= dx; i++) {
                pxSafe(x1 + i * sx, y);
                err += dy;
                if (err >= dx) { y += sy; err -= dx; }
            }
        } else {
            int x = x1, err = dx - dy / 2;
            for (int i = 0; i <= dy; i++) {
                pxSafe(x, y1 + i * sy);
                err += dx;
                if (err >= dy) { x += sx; err -= dy; }
            }
        }
    }

    /* ---- 矩形 ---- */
    public void drawRect(int x, int y, int w, int h) {
        if (w < 0 || h < 0) return;
        drawLine(x, y, x + w, y);
        drawLine(x, y + h, x + w, y + h);
        drawLine(x, y, x, y + h);
        drawLine(x + w, y, x + w, y + h);
    }

    public void fillRect(int x, int y, int w, int h) {
        int ax = Math.max(clipX, transX + x);
        int ay = Math.max(clipY, transY + y);
        int bx = Math.min(clipX + clipW, transX + x + Math.max(w, 0));
        int by = Math.min(clipY + clipH, transY + y + Math.max(h, 0));
        int c = 0xFF000000 | color;
        for (int yy = ay; yy < by; yy++) {
            int row = yy * stride;
            for (int xx = ax; xx < bx; xx++) data[row + xx] = c;
        }
    }

    public void drawRoundRect(int x, int y, int w, int h, int arcW, int arcH) {
        if (w < 0 || h < 0) return;
        int aw = Math.min(arcW / 2, w / 2), ah = Math.min(arcH / 2, h / 2);
        if (aw <= 0 || ah <= 0) { drawRect(x, y, w, h); return; }
        drawLine(x + aw, y, x + w - aw, y);
        drawLine(x + aw, y + h, x + w - aw, y + h);
        drawLine(x, y + ah, x, y + h - ah);
        drawLine(x + w, y + ah, x + w, y + h - ah);
        drawArc(x, y, aw * 2, ah * 2, 90, 90);
        drawArc(x + w - aw * 2, y, aw * 2, ah * 2, 0, 90);
        drawArc(x, y + h - ah * 2, aw * 2, ah * 2, 180, 90);
        drawArc(x + w - aw * 2, y + h - ah * 2, aw * 2, ah * 2, 270, 90);
    }

    public void fillRoundRect(int x, int y, int w, int h, int arcW, int arcH) {
        if (w <= 0 || h <= 0) return;
        int aw = Math.min(arcW / 2, w / 2), ah = Math.min(arcH / 2, h / 2);
        if (aw <= 0 || ah <= 0) { fillRect(x, y, w, h); return; }
        fillRect(x + aw, y, w - aw * 2, h);
        fillRect(x, y + ah, aw, h - ah * 2);
        fillRect(x + w - aw, y + ah, aw, h - ah * 2);
        fillArc(x, y, aw * 2, ah * 2, 90, 90);
        fillArc(x + w - aw * 2, y, aw * 2, ah * 2, 0, 90);
        fillArc(x, y + h - ah * 2, aw * 2, ah * 2, 180, 90);
        fillArc(x + w - aw * 2, y + h - ah * 2, aw * 2, ah * 2, 270, 90);
    }

    /* ---- 弧（角度制，0=3点钟方向，逆时针为正——J2ME 坐标系 y 向下，视觉上顺时针） ---- */
    public void drawArc(int x, int y, int w, int h, int startAngle, int arcAngle) {
        if (w <= 0 || h <= 0 || arcAngle == 0) return;
        int cx = transX + x + w / 2, cy = transY + y + h / 2;
        int rx = w / 2, ry = h / 2;
        if (rx <= 0 || ry <= 0) return;
        double a0 = Math.toRadians(startAngle);
        double a1 = Math.toRadians(startAngle + arcAngle);
        int steps = Math.max(rx, ry) * 4;
        for (int i = 0; i <= steps; i++) {
            double t = a0 + (a1 - a0) * i / steps;
            int px = cx + (int) Math.round(rx * Math.cos(t));
            int py = cy + (int) Math.round(ry * Math.sin(t));
            pxSafe(px, py);
        }
    }

    public void fillArc(int x, int y, int w, int h, int startAngle, int arcAngle) {
        if (w <= 0 || h <= 0 || arcAngle == 0) return;
        int cx = transX + x + w / 2, cy = transY + y + h / 2;
        int rx = w / 2, ry = h / 2;
        if (rx <= 0 || ry <= 0) return;
        double a0 = Math.toRadians(startAngle);
        double a1 = Math.toRadians(startAngle + arcAngle);
        boolean full = Math.abs(arcAngle) >= 360;
        /* 逐行扫描：对每 y 求与椭圆弧段的交点区间 */
        int c = 0xFF000000 | color;
        for (int py = cy - ry; py <= cy + ry; py++) {
            if (py < clipY || py >= clipY + clipH) continue;
            double dy = (double) (py - cy) / ry;
            if (dy < -1 || dy > 1) continue;
            double xo = rx * Math.sqrt(1 - dy * dy);
            int xl = (int) Math.floor(cx - xo), xr = (int) Math.ceil(cx + xo);
            if (full) {
                fillRow(py, Math.max(xl, clipX), Math.min(xr, clipX + clipW), c);
                continue;
            }
            /* 求角度范围（含跨 0 处理） */
            double th = Math.asin(dy);
            double aA = -th;             /* 右半交点角 */
            double aB = Math.PI + th;    /* 左半交点角 */
            /* 检查 [a0,a1] 与各候选区间（右半 [aA], 左半 [aB]，及 +2π 平移） */
            int x1 = -1, x2 = -1;
            double[] cands = { aA, aB, aA + 2 * Math.PI, aB + 2 * Math.PI, aA - 2 * Math.PI, aB - 2 * Math.PI };
            double lo = Math.min(a0, a1), hi = Math.max(a0, a1);
            for (int i = 0; i < cands.length; i += 2) {
                /* 每对（右半,左半）构成一个连通穿越：从 cands[i] 到 cands[i+1] 沿弧 */
                double s = cands[i], e = cands[i + 1];
                if (e < s) { double tmp = s; s = e; e = tmp; }
                double ms = Math.max(s, lo), me = Math.min(e, hi);
                if (ms <= me) {
                    double xs = cx + rx * Math.cos(ms);
                    double xe = cx + rx * Math.cos(me);
                    int ixs = (int) Math.round(xs), ixe = (int) Math.round(xe);
                    if (ixs > ixe) { int tmp = ixs; ixs = ixe; ixe = tmp; }
                    if (x1 < 0) { x1 = ixs; x2 = ixe; }
                    else {
                        x1 = Math.min(x1, ixs);
                        x2 = Math.max(x2, ixe);
                    }
                }
            }
            if (x1 >= 0) fillRow(py, Math.max(x1, clipX), Math.min(x2 + 1, clipX + clipW), c);
        }
    }

    private void fillRow(int py, int x1, int x2, int c) {
        if (x2 <= x1) return;
        int row = py * stride;
        for (int xx = x1; xx < x2; xx++) data[row + xx] = c;
    }

    public void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
        x1 += transX; y1 += transY; x2 += transX; y2 += transY; x3 += transX; y3 += transY;
        int minY = Math.min(Math.min(y1, y2), y3), maxY = Math.max(Math.max(y1, y2), y3);
        int c = 0xFF000000 | color;
        for (int y = minY; y <= maxY; y++) {
            int xa = crossX(x1, y1, x2, y2, y), xb = crossX(x2, y2, x3, y3, y), xc = crossX(x3, y3, x1, y1, y);
            int lo = Math.min(Math.min(xa, xb), xc), hi = Math.max(Math.max(xa, xb), xc);
            if (lo == Integer.MAX_VALUE) continue;
            lo = Math.max(lo, clipX); hi = Math.min(hi, clipX + clipW);
            if (y < clipY || y >= clipY + clipH) continue;
            fillRow(y, lo, Math.min(hi + 1, clipX + clipW), c);
        }
    }

    private static int crossX(int x1, int y1, int x2, int y2, int y) {
        if (y1 == y2) return (y >= Math.min(y1, y2) && y <= Math.max(y1, y2)) ? Math.min(x1, x2) : Integer.MAX_VALUE;
        double t = (double) (y - y1) / (y2 - y1);
        if (t < 0 || t > 1) return Integer.MAX_VALUE;
        return (int) Math.round(x1 + t * (x2 - x1));
    }

    /* ---- 文本（native 点阵光栅） ---- */
    public void drawString(String s, int x, int y, int anchor) {
        drawText(s, 0, s == null ? 0 : s.length(), x, y, anchor);
    }

    public void drawSubstring(String s, int off, int len, int x, int y, int anchor) {
        if (s == null) throw new NullPointerException();
        if (off < 0 || off > s.length() || len < 0 || off + len > s.length())
            throw new StringIndexOutOfBoundsException();
        drawText(s, off, len, x, y, anchor);
    }

    public void drawChar(char c, int x, int y, int anchor) {
        char[] a = { c };
        drawChars(a, 0, 1, x, y, anchor);
    }

    public void drawChars(char[] c, int off, int len, int x, int y, int anchor) {
        if (c == null) throw new NullPointerException();
        if (off < 0 || off > c.length || len < 0 || off + len > c.length)
            throw new ArrayIndexOutOfBoundsException();
        String s = new String(c, off, len);
        drawText(s, 0, len, x, y, anchor);
    }

    private void drawText(String s, int off, int len, int x, int y, int anchor) {
        if (s == null) s = "null";
        if (len <= 0) return;
        Font f = font;
        int w = f.substringWidth(s, off, len);
        int h = f.getHeight();
        int ax = transX + x, ay = transY + y;
        switch (anchor & 0xF0) {
            case BOTTOM: ay -= h; break;
            case BASELINE: ay -= f.getBaselinePosition(); break;
            case TOP: default: break;
        }
        switch (anchor & 0x0F) {
            case HCENTER: ax -= w / 2; break;
            case RIGHT: ax -= w; break;
            case LEFT: default: break;
        }
        String sub = (off == 0 && len == s.length()) ? s : s.substring(off, off + len);
        int bold = (f.getStyle() & Font.STYLE_BOLD) != 0 ? 1 : 0;
        rasterText(data, stride, ax, ay, color, clipX, clipY, clipW, clipH, sub, bold);
    }

    /** C 端 8x12 点阵光栅（Bridge/headless 注册） */
    static native void rasterText(int[] data, int stride, int x, int y, int color,
                                  int clipX, int clipY, int clipW, int clipH,
                                  String s, int bold);

    /* ---- 图像 ---- */
    public void drawImage(Image img, int x, int y, int anchor) {
        if (img == null) throw new NullPointerException();
        int ax = transX + x, ay = transY + y;
        int w = img.getWidth(), h = img.getHeight();
        switch (anchor & 0xF0) {
            case BOTTOM: ay -= h; break;
            case VCENTER: ay -= h / 2; break;
            case TOP: default: break;
        }
        switch (anchor & 0x0F) {
            case HCENTER: ax -= w / 2; break;
            case RIGHT: ax -= w; break;
            case LEFT: default: break;
        }
        blit(img.rgb, img.w, img.h, 0, 0, w, h, ax, ay, 0);
    }

    public void drawRegion(Image src, int x_src, int y_src, int width, int height,
                           int transform, int x_dest, int y_dest, int anchor) {
        if (src == null) throw new NullPointerException();
        if (x_src < 0 || y_src < 0 || width <= 0 || height <= 0
                || x_src + width > src.w || y_src + height > src.h)
            throw new IllegalArgumentException();
        int w = width, h = height;
        switch (transform) {
            case Sprite.TRANS_ROT90: case Sprite.TRANS_ROT270:
            case Sprite.TRANS_MIRROR_ROT90: case Sprite.TRANS_MIRROR_ROT270:
                w = height; h = width; break;
        }
        int ax = transX + x_dest, ay = transY + y_dest;
        switch (anchor & 0xF0) {
            case BOTTOM: ay -= h; break;
            case VCENTER: ay -= h / 2; break;
            case TOP: default: break;
        }
        switch (anchor & 0x0F) {
            case HCENTER: ax -= w / 2; break;
            case RIGHT: ax -= w; break;
            case LEFT: default: break;
        }
        /* 变换逐像素（源->目标），小区域性能可接受 */
        for (int dy = 0; dy < h; dy++) {
            int py = ay + dy;
            if (py < clipY || py >= clipY + clipH) continue;
            for (int dx = 0; dx < w; dx++) {
                int px = ax + dx;
                if (px < clipX || px >= clipX + clipW) continue;
                int sx, sy;
                switch (transform) {
                    case Sprite.TRANS_MIRROR: sx = x_src + width - 1 - dx; sy = y_src + dy; break;
                    case Sprite.TRANS_ROT90: sx = x_src + dy; sy = y_src + height - 1 - dx; break;
                    case Sprite.TRANS_ROT180: sx = x_src + width - 1 - dx; sy = y_src + height - 1 - dy; break;
                    case Sprite.TRANS_ROT270: sx = x_src + width - 1 - dy; sy = y_src + dx; break;
                    case Sprite.TRANS_MIRROR_ROT90: sx = x_src + width - 1 - dy; sy = y_src + height - 1 - dx; break;
                    case Sprite.TRANS_MIRROR_ROT180: sx = x_src + dx; sy = y_src + height - 1 - dy; break;
                    case Sprite.TRANS_MIRROR_ROT270: sx = x_src + dy; sy = y_src + dx; break;
                    default: sx = x_src + dx; sy = y_src + dy; break;
                }
                int p = src.rgb[sy * src.w + sx];
                int a = (p >>> 24);
                if (a == 0) continue;
                if (a == 255) { data[py * stride + px] = p | 0xFF000000; continue; }
                int br = (p >> 16) & 0xFF, bg = (p >> 8) & 0xFF, bb = p & 0xFF;
                int d = data[py * stride + px];
                int dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
                int r = (br * a + dr * (255 - a)) / 255;
                int g = (bg * a + dg * (255 - a)) / 255;
                int b = (bb * a + db * (255 - a)) / 255;
                data[py * stride + px] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }

    public void copyArea(int x_src, int y_src, int width, int height,
                         int x_dest, int y_dest, int anchor) {
        throw new IllegalStateException("copyArea needs Canvas target");
    }

    /* 直接像素复制（不透明覆盖 + alpha 混合） */
    void blit(int[] src, int sw, int sh, int sx, int sy, int w, int h, int dx, int dy, int ropAlpha) {
        int x1 = Math.max(dx, clipX), x2 = Math.min(dx + w, clipX + clipW);
        int y1 = Math.max(dy, clipY), y2 = Math.min(dy + h, clipY + clipH);
        if (x2 <= x1 || y2 <= y1) return;
        int offS = x1 - dx, offY = y1 - dy;
        for (int yy = y1; yy < y2; yy++) {
            int srow = (sy + offY + yy - y1) * sw + sx + offS;
            int drow = yy * stride + x1;
            for (int xx = x1; xx < x2; xx++) {
                int p = src[srow++];
                int a = (p >>> 24);
                if (a == 0) { drow++; continue; }
                if (a == 255) { data[drow++] = p | 0xFF000000; continue; }
                int br = (p >> 16) & 0xFF, bg = (p >> 8) & 0xFF, bb = p & 0xFF;
                int d = data[drow];
                int dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
                int r = (br * a + dr * (255 - a)) / 255;
                int g = (bg * a + dg * (255 - a)) / 255;
                int b = (bb * a + db * (255 - a)) / 255;
                data[drow++] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }
}
