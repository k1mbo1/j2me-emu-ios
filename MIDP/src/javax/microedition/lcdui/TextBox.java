package javax.microedition.lcdui;

public class TextBox extends Screen {

    private String text = "";
    private int maxSize;
    private int constraints;
    private int caret;

    public TextBox(String title, String text, int maxSize, int constraints) {
        super(title);
        if (maxSize <= 0) throw new IllegalArgumentException();
        this.maxSize = maxSize;
        this.constraints = constraints;
        setString(text == null ? "" : text);
    }

    public void setString(String text) {
        if (text == null) text = "";
        if (text.length() > maxSize) text = text.substring(0, maxSize);
        this.text = text;
        caret = text.length();
        repaintScreen();
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
    public void setConstraints(int c) { constraints = c; }

    public void insert(String src, int pos) {
        if (pos < 0 || pos > text.length()) throw new IndexOutOfBoundsException();
        String nt = text.substring(0, pos) + src + text.substring(pos);
        if (nt.length() > maxSize) nt = nt.substring(0, maxSize);
        text = nt;
        caret = Math.min(pos + src.length(), text.length());
        repaintScreen();
    }

    public void delete(int offset, int length) {
        if (offset < 0 || length < 0 || offset + length > text.length())
            throw new StringIndexOutOfBoundsException();
        text = text.substring(0, offset) + text.substring(offset + length);
        caret = Math.min(caret, text.length());
        repaintScreen();
    }

    public int getCaretPosition() { return caret; }

    int paintContent(Graphics g, int x, int y, int w, int h) {
        Font font = Font.getDefaultFont();
        int lineH = font.getHeight() + 2;
        int cw = font.charWidth('x');
        int maxCols = Math.max((w - 12) / cw, 1);
        /* 按行折行绘制 */
        int pos = 0, cy = 4;
        int caretLine = -1, caretCol = -1;
        int before = 0;
        while (pos < text.length() || (pos == 0 && text.length() == 0)) {
            int end = Math.min(pos + maxCols, text.length());
            if (caret >= pos && caret <= end) {
                caretLine = cy;
                caretCol = caret - pos;
            }
            String line = text.substring(pos, end);
            g.drawString(line, x + 6, y + cy, Graphics.LEFT | Graphics.TOP);
            pos = end;
            cy += lineH;
            if (pos >= text.length()) break;
        }
        /* 光标 */
        if (caretLine >= 0) {
            int cx = x + 6 + caretCol * cw;
            g.setColor(0x1050A0);
            g.drawLine(cx, y + caretLine, cx, y + caretLine + font.getHeight());
        }
        g.setColor(0);
        return cy + 8;
    }

    boolean contentKey(int keyCode) {
        if (keyCode == 8) {
            if (caret > 0) { delete(caret - 1, 1); caret--; }
            return true;
        }
        if (keyCode >= 32 && keyCode < 127) {
            if (text.length() < maxSize) insert(String.valueOf((char) keyCode), caret);
            return true;
        }
        return false;
    }

    void onSoftLeft() {
        if (nCommands > 0) fireCommand(commands[0]);
    }

    void onSoftRight() {
        for (int i = 0; i < nCommands; i++)
            if (commands[i].getCommandType() == Command.BACK
                    || commands[i].getCommandType() == Command.EXIT
                    || commands[i].getCommandType() == Command.CANCEL) {
                fireCommand(commands[i]);
                return;
            }
        if (nCommands > 0) fireCommand(commands[nCommands - 1]);
    }

    String softLeftLabel() { return nCommands > 0 ? commands[0].getLabel() : null; }
    String softRightLabel() { return null; }
}
