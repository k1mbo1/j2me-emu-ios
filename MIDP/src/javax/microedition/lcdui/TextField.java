package javax.microedition.lcdui;

public class TextField extends Item {

    public static final int ANY = 0, EMAILADDR = 1, NUMERIC = 2, PHONENUMBER = 3,
            URL = 4, DECIMAL = 5;
    public static final int PASSWORD = 0x10000, UNEDITABLE = 0x20000,
            SENSITIVE = 0x40000, NON_PREDICTIVE = 0x80000,
            INITIAL_CAPS_WORD = 0x100000, INITIAL_CAPS_SENTENCE = 0x200000;

    private String text = "";
    private int maxSize;
    private int constraints;
    private int caret;
    private boolean editing;

    public TextField(String label, String text, int maxSize, int constraints) {
        super(label);
        if (maxSize <= 0) throw new IllegalArgumentException();
        this.maxSize = maxSize;
        this.constraints = constraints;
        setText(text == null ? "" : text);
    }

    public void setString(String text) { setText(text == null ? "" : text); }

    public void setText(String text) {
        if (text == null) text = "";
        if (text.length() > maxSize) text = text.substring(0, maxSize);
        this.text = text;
        this.caret = text.length();
    }

    public String getString() { return text; }
    public int size() { return text.length(); }
    public int getMaxSize() { return maxSize; }

    public int setMaxSize(int maxSize) {
        if (maxSize <= 0) throw new IllegalArgumentException();
        this.maxSize = maxSize;
        if (text.length() > maxSize) text = text.substring(0, maxSize);
        return maxSize;
    }

    public int getConstraints() { return constraints & 0xFFFF; }
    public void setConstraints(int c) {
        this.constraints = (this.constraints & 0xFFF0000) | (c & 0xFFFF);
    }

    public char[] getChars(char[] ret) {
        if (ret == null || ret.length < text.length())
            ret = new char[Math.max(text.length(), 1)];
        for (int i = 0; i < text.length(); i++) ret[i] = text.charAt(i);
        return ret;
    }

    public void insert(String src, int pos) {
        if (pos < 0 || pos > text.length()) throw new IndexOutOfBoundsException();
        String nt = text.substring(0, pos) + src + text.substring(pos);
        if (nt.length() > maxSize) nt = nt.substring(0, maxSize);
        text = nt;
        caret = Math.min(pos + src.length(), text.length());
    }

    public void insert(char[] data, int offset, int length, int pos) {
        insert(new String(data, offset, length), pos);
    }

    public void delete(int offset, int length) {
        if (offset < 0 || length < 0 || offset + length > text.length())
            throw new StringIndexOutOfBoundsException();
        text = text.substring(0, offset) + text.substring(offset + length);
        caret = Math.min(caret, text.length());
    }

    public int getCaretPosition() { return caret; }

    int contentHeight(Form f) {
        Font font = Font.getDefaultFont();
        return (label != null ? font.getHeight() + 2 : 0) + font.getHeight() + 10;
    }

    void paintContent(Graphics g, int x, int y, int w, Form f) {
        Font font = Font.getDefaultFont();
        if (label != null && label.length() > 0) {
            g.setColor(0);
            g.drawString(label, x, y, Graphics.LEFT | Graphics.TOP);
            y += font.getHeight() + 2;
        }
        /* 输入框 */
        g.setColor(f.focus == this ? 0x1050A0 : 0x808080);
        g.drawRect(x, y, w - 4, font.getHeight() + 6);
        g.setColor(0);
        String shown = text;
        if ((constraints & PASSWORD) != 0) {
            StringBuffer sb = new StringBuffer();
            for (int i = 0; i < text.length(); i++) sb.append('*');
            shown = sb.toString();
        }
        /* 超宽截断显示（尾部） */
        int cw = font.charWidth('x');
        int maxChars = Math.max((w - 12) / cw, 1);
        if (shown.length() > maxChars) shown = shown.substring(shown.length() - maxChars);
        g.drawString(shown, x + 6, y + 3, Graphics.LEFT | Graphics.TOP);
        if (f.focus == this && editing) {
            int cx = x + 6 + Math.min(caret, maxChars) * cw;
            g.drawLine(cx, y + 2, cx, y + font.getHeight() + 4);
        }
    }

    /* 键输入（Screen 转发；可打印字符 keyCode == char） */
    boolean keyInput(int keyCode) {
        if ((constraints & UNEDITABLE) != 0) return false;
        if (keyCode == 8) {   /* BACKSPACE */
            if (caret > 0) { delete(caret - 1, 1); caret--; }
            return true;
        }
        if (keyCode >= 32 && keyCode < 127) {
            char c = (char) keyCode;
            if (c == '"' || c == '\\') return true;   /* 字符串内无法输入 */
            if (text.length() < maxSize) {
                insert(String.valueOf(c), caret);
                return true;
            }
        }
        return false;
    }
}
