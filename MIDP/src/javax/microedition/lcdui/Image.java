package javax.microedition.lcdui;

import java.io.IOException;
import javax.microedition.lcdui.game.Sprite;

/**
 * Image：int[] ARGB 像素。可变 Image 持有 Graphics。
 * PNG 解码走 native decodePng（Bridge；headless 注册假实现）。
 */
public class Image {

    /** 像素数据（ARGB）。public 供 game 包高速访问（自有实现扩展） */
    public int[] rgb;
    int w, h;
    private Graphics graphics;
    private boolean mutable;

    private Image() { }

    /* ---- 创建 ---- */
    public static Image createImage(int width, int height) {
        if (width <= 0 || height <= 0) throw new IllegalArgumentException();
        Image img = new Image();
        img.w = width; img.h = height;
        img.rgb = new int[width * height];   /* 0 = 全透明 */
        img.mutable = true;
        return img;
    }

    public static Image createRGBImage(int[] rgbData, int width, int height, boolean processAlpha) {
        if (rgbData == null) throw new NullPointerException();
        if (width <= 0 || height <= 0 || rgbData.length < width * height)
            throw new IllegalArgumentException();
        Image img = new Image();
        img.w = width; img.h = height;
        img.rgb = new int[width * height];
        if (processAlpha) {
            System.arraycopy(rgbData, 0, img.rgb, 0, width * height);
        } else {
            for (int i = 0; i < width * height; i++)
                img.rgb[i] = 0xFF000000 | rgbData[i];
        }
        return img;
    }

    public static Image createImage(String name) throws IOException {
        if (name == null) throw new NullPointerException();
        byte[] data = bridgeReadResource(name);
        if (data == null) throw new IOException("resource not found: " + name);
        Image img = decodePng(data);
        if (img == null) throw new IOException("png decode failed: " + name);
        return img;
    }

    public static Image createImage(byte[] imageData, int imageOffset, int imageLength) throws IOException {
        if (imageData == null) throw new NullPointerException();
        if (imageOffset < 0 || imageLength < 0 || imageOffset + imageLength > imageData.length)
            throw new ArrayIndexOutOfBoundsException();
        byte[] d = new byte[imageLength];
        System.arraycopy(imageData, imageOffset, d, 0, imageLength);
        Image img = decodePng(d);
        if (img == null) throw new IOException("png decode failed");
        return img;
    }

    public static Image createImage(Image source) {
        if (source == null) throw new NullPointerException();
        if (source.isMutable()) {
            Image img = createImage(source.w, source.h);
            System.arraycopy(source.rgb, 0, img.rgb, 0, source.w * source.h);
            return img;
        }
        Image img = new Image();
        img.w = source.w; img.h = source.h;
        img.rgb = source.rgb;   /* 不可变可共享 */
        return img;
    }

    public static Image createImage(int[] rgbData, int width, int height, boolean processAlpha) {
        return createRGBImage(rgbData, width, height, processAlpha);
    }

    /** 区域裁剪 + 变换创建（精灵图拆分常用；transformation 取 Sprite.TRANS_* 常量） */
    public static Image createImage(Image source, int x, int y, int width, int height,
                                    int transformation) {
        if (source == null) throw new NullPointerException();
        if (width <= 0 || height <= 0
                || x < 0 || y < 0 || x + width > source.w || y + height > source.h)
            throw new IllegalArgumentException();
        boolean rotated = transformation == Sprite.TRANS_ROT90
                || transformation == Sprite.TRANS_ROT270
                || transformation == Sprite.TRANS_MIRROR_ROT90
                || transformation == Sprite.TRANS_MIRROR_ROT270;
        int w2 = rotated ? height : width;
        int h2 = rotated ? width : height;
        Image img = createImage(w2, h2);
        img.getGraphics().drawRegion(source, x, y, width, height, transformation,
                0, 0, Graphics.TOP | Graphics.LEFT);
        img.mutable = false;   /* 语义：返回不可变 Image */
        return img;
    }

    /* ---- 查询 ---- */
    public int getWidth() { return w; }
    public int getHeight() { return h; }
    public boolean isMutable() { return mutable; }

    public Graphics getGraphics() {
        if (!mutable) throw new IllegalStateException("immutable image");
        if (graphics == null) graphics = new Graphics(rgb, w, w, h);
        return graphics;
    }

    public void getRGB(int[] rgbData, int offset, int scanlength,
                       int x, int y, int width, int height) {
        if (rgbData == null) throw new NullPointerException();
        if (width <= 0 || height <= 0) return;
        if (x < 0 || y < 0 || x + width > w || y + height > h
                || offset < 0 || offset + (height - 1) * scanlength + width > rgbData.length
                || scanlength < width && false)
            throw new IllegalArgumentException();
        for (int row = 0; row < height; row++)
            System.arraycopy(rgb, (y + row) * w + x, rgbData, offset + row * scanlength, width);
    }

    /* ---- natives ---- */
    public static native byte[] bridgeReadResource(String name);
    static native Image decodePng(byte[] data);
}
