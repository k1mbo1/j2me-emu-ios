package javax.microedition.lcdui;

/**
 * 字体骨架：统一 8x12 点阵（C 端 rasterText 光栅）。
 * face/size 影响不大，v1.0.0 统一尺寸（J2ME 应用多数用默认字体）。
 */
public final class Font {

    public static final int STYLE_PLAIN = 0, STYLE_BOLD = 1, STYLE_ITALIC = 2,
            STYLE_UNDERLINED = 4;
    public static final int FACE_SYSTEM = 0, FACE_MONOSPACE = 32, FACE_PROPORTIONAL = 64;
    public static final int SIZE_SMALL = 8, SIZE_MEDIUM = 0, SIZE_LARGE = 16;

    static final Font DEFAULT = new Font(FACE_SYSTEM, STYLE_PLAIN, SIZE_MEDIUM);

    final int face, style, size;
    final int charW, charH, ascent;

    private Font(int face, int style, int size) {
        this.face = face;
        this.style = style;
        this.size = size;
        int scale = size == SIZE_SMALL ? 1 : (size == SIZE_LARGE ? 2 : 1);
        this.charW = 8;
        this.charH = 12;
        this.ascent = 10;
        /* v1.0.0 统一 8x12，scale 暂不影响 */
    }

    public static Font getDefaultFont() { return DEFAULT; }

    public static Font getFont(int face, int style, int size) {
        return new Font(face, style, size);
    }

    public static int getFont(int face, int style) { return SIZE_MEDIUM; }

    public int getFace() { return face; }
    public int getStyle() { return style; }
    public int getSize() { return size; }
    public boolean isPlain() { return style == STYLE_PLAIN; }
    public boolean isBold() { return (style & STYLE_BOLD) != 0; }
    public boolean isItalic() { return (style & STYLE_ITALIC) != 0; }
    public boolean isUnderlined() { return (style & STYLE_UNDERLINED) != 0; }

    public int getHeight() { return charH; }
    public int getBaselinePosition() { return ascent; }

    public int charWidth(char c) { return charW; }
    public int charsWidth(char[] c, int off, int len) {
        if (c == null) throw new NullPointerException();
        if (off < 0 || off > c.length || len < 0 || off + len > c.length)
            throw new ArrayIndexOutOfBoundsException();
        return len * charW;
    }
    public int stringWidth(String s) {
        if (s == null) throw new NullPointerException();
        return s.length() * charW;
    }
    public int substringWidth(String s, int off, int len) {
        if (s == null) throw new NullPointerException();
        if (off < 0 || off > s.length() || len < 0 || off + len > s.length())
            throw new StringIndexOutOfBoundsException();
        return len * charW;
    }
}
