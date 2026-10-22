package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class Sprite extends Layer {

    /* 变换常量（Graphics.drawRegion 依赖） */
    public static final int TRANS_NONE = 0;
    public static final int TRANS_ROT90 = 5;
    public static final int TRANS_ROT180 = 3;
    public static final int TRANS_ROT270 = 6;
    public static final int TRANS_MIRROR = 2;
    public static final int TRANS_MIRROR_ROT90 = 7;
    public static final int TRANS_MIRROR_ROT180 = 1;
    public static final int TRANS_MIRROR_ROT270 = 4;

    private Image image;
    private int fW, fH;            /* 帧尺寸 */
    private int[] seq;             /* 帧序列 */
    private int seqIdx;
    private int rawFrame;          /* 无序列时的当前帧 */
    private int refX, refY;        /* 参考点 */
    private int transform = TRANS_NONE;
    private int collX, collY, collW, collH;   /* 碰撞矩形（帧内） */

    public Sprite(Image image) {
        super(image == null ? 0 : image.getWidth(), image == null ? 0 : image.getHeight());
        initImage(image);
        this.fW = width;
        this.fH = height;
        this.collX = 0; this.collY = 0; this.collW = fW; this.collH = fH;
    }

    public Sprite(Image image, int frameWidth, int frameHeight) {
        super(frameWidth, frameHeight);
        initImage(image);
        this.fW = frameWidth;
        this.fH = frameHeight;
        if (image != null && (frameWidth <= 0 || frameHeight <= 0
                || image.getWidth() % frameWidth != 0
                || image.getHeight() % frameHeight != 0))
            throw new IllegalArgumentException();
        this.collX = 0; this.collY = 0; this.collW = fW; this.collH = fH;
    }

    public Sprite(Sprite s) {
        super(s.width, s.height);
        this.image = s.image;
        this.fW = s.fW; this.fH = s.fH;
        this.seq = s.seq == null ? null : copySeq(s.seq);
        this.seqIdx = s.seqIdx;
        this.rawFrame = s.rawFrame;
        this.refX = s.refX; this.refY = s.refY;
        this.transform = s.transform;
        this.collX = s.collX; this.collY = s.collY;
        this.collW = s.collW; this.collH = s.collH;
    }

    private static int[] copySeq(int[] s) {
        int[] r = new int[s.length];
        System.arraycopy(s, 0, r, 0, s.length);
        return r;
    }

    private void initImage(Image img) {
        if (img == null) throw new NullPointerException();
        if (img.isMutable()) throw new IllegalArgumentException("mutable image");
        this.image = img;
    }

    public void setImage(Image img, int frameWidth, int frameHeight) {
        initImage(img);
        this.fW = frameWidth;
        this.fH = frameHeight;
        if (seq != null) {
            int cols = img.getWidth() / frameWidth;
            int rows = img.getHeight() / frameHeight;
            for (int i = 0; i < seq.length; i++)
                if (seq[i] >= cols * rows) throw new IllegalArgumentException();
        }
        width = (transform == TRANS_ROT90 || transform == TRANS_ROT270
                || transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270)
                ? frameHeight : frameWidth;
        height = (transform == TRANS_ROT90 || transform == TRANS_ROT270
                || transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270)
                ? frameWidth : frameHeight;
    }

    public void defineReferencePixel(int x, int y) { refX = x; refY = y; }
    public void setRefPixelPosition(int x, int y) {
        this.x = x - refX;
        this.y = y - refY;
    }
    public int getRefPixelX() { return x + refX; }
    public int getRefPixelY() { return y + refY; }

    public void setFrame(int seqIndex) {
        if (seq != null) {
            if (seqIndex < 0 || seqIndex >= seq.length) throw new IndexOutOfBoundsException();
            seqIdx = seqIndex;
        } else {
            int cols = image.getWidth() / fW;
            if (seqIndex < 0 || seqIndex >= cols * (image.getHeight() / fH))
                throw new IndexOutOfBoundsException();
            rawFrame = seqIndex;
        }
    }

    public final int getFrame() { return seq != null ? seqIdx : rawFrame; }
    public final int getFrameSequenceLength() {
        if (seq != null) return seq.length;
        return (image.getWidth() / fW) * (image.getHeight() / fH);
    }
    public void nextFrame() { setFrame((getFrame() + 1) % getFrameSequenceLength()); }
    public void prevFrame() {
        int n = getFrameSequenceLength();
        setFrame((getFrame() + n - 1) % n);
    }

    public void setFrameSequence(int[] sequence) {
        if (sequence == null) { seq = null; return; }
        if (sequence.length == 0) throw new IllegalArgumentException();
        int cols = image.getWidth() / fW;
        int rows = image.getHeight() / fH;
        for (int i = 0; i < sequence.length; i++)
            if (sequence[i] < 0 || sequence[i] >= cols * rows)
                throw new ArrayIndexOutOfBoundsException();
        seq = copySeq(sequence);
        seqIdx = 0;
    }

    public void setTransform(int transform) {
        this.transform = transform;
        boolean rotated = transform == TRANS_ROT90 || transform == TRANS_ROT270
                || transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270;
        width = rotated ? fH : fW;
        height = rotated ? fW : fH;
    }

    public void defineCollisionRectangle(int x, int y, int width, int height) {
        collX = x; collY = y; collW = width; collH = height;
    }

    public final boolean collidesWith(Sprite s, boolean pixelLevel) {
        if (!visible || !s.visible) return false;
        int ax1 = x + collX, ay1 = y + collY, ax2 = ax1 + collW, ay2 = ay1 + collH;
        int bx1 = s.x + s.collX, by1 = s.y + s.collY, bx2 = bx1 + s.collW, by2 = by1 + s.collH;
        if (ax2 <= bx1 || bx2 <= ax1 || ay2 <= by1 || by2 <= ay1) return false;
        if (!pixelLevel) return true;
        int ix1 = Math.max(ax1, bx1), iy1 = Math.max(ay1, by1);
        int ix2 = Math.min(ax2, bx2), iy2 = Math.min(ay2, by2);
        for (int py = iy1; py < iy2; py++) {
            for (int px = ix1; px < ix2; px++) {
                int p1 = pixelAt(this, px, py);
                int p2 = pixelAt(s, px, py);
                if ((p1 >>> 24) != 0 && (p2 >>> 24) != 0) return true;
            }
        }
        return false;
    }

    private static int pixelAt(Sprite sp, int worldX, int worldY) {
        int fx = worldX - sp.x - sp.collX;
        int fy = worldY - sp.y - sp.collY;
        int cols = sp.image.getWidth() / sp.fW;
        int frame = sp.seq != null ? sp.seq[sp.seqIdx] : sp.rawFrame;
        int col = frame % cols, row = frame / cols;
        int sx = col * sp.fW + fx, sy = row * sp.fH + fy;
        if (sx < 0 || sy < 0 || sx >= sp.image.getWidth() || sy >= sp.image.getHeight())
            return 0;
        return sp.image.rgb[sy * sp.image.getWidth() + sx];
    }

    public final boolean collidesWith(TiledLayer t, boolean pixelLevel) {
        if (!visible || !t.visible) return false;
        return t.collidesRect(x + collX, y + collY, collW, collH);
    }

    public final boolean collidesWith(Image image, int x, int y, boolean pixelLevel) {
        if (image == null) throw new NullPointerException();
        if (!visible) return false;
        int ax1 = this.x + collX, ay1 = this.y + collY, ax2 = ax1 + collW, ay2 = ay1 + collH;
        int bx2 = x + image.getWidth(), by2 = y + image.getHeight();
        if (ax2 <= x || bx2 <= ax1 || ay2 <= y || by2 <= ay1) return false;
        if (!pixelLevel) return true;
        int ix1 = Math.max(ax1, x), iy1 = Math.max(ay1, y);
        int ix2 = Math.min(ax2, bx2), iy2 = Math.min(ay2, by2);
        for (int py = iy1; py < iy2; py++)
            for (int px = ix1; px < ix2; px++) {
                int p1 = pixelAt(this, px, py);
                int p2 = image.rgb[(py - y) * image.getWidth() + (px - x)];
                if ((p1 >>> 24) != 0 && (p2 >>> 24) != 0) return true;
            }
        return false;
    }

    public void paint(Graphics g) {
        if (!visible) return;
        int cols = image.getWidth() / fW;
        int frame = seq != null ? seq[seqIdx] : rawFrame;
        int col = frame % cols, row = frame / cols;
        g.drawRegion(image, col * fW, row * fH, fW, fH, transform, x, y, Graphics.TOP | Graphics.LEFT);
    }
}
