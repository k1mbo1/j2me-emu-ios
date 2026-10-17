package javax.microedition.lcdui;

public class StringItem extends Item {

    private String text;
    private int appearanceMode;

    public StringItem(String label, String text) {
        this(label, text, PLAIN);
    }

    public StringItem(String label, String text, int appearanceMode) {
        super(label);
        this.text = text;
        this.appearanceMode = appearanceMode;
    }

    public void setText(String text) { this.text = text; }
    public String getText() { return text; }
    public int getAppearanceMode() { return appearanceMode; }

    public Font getDefaultFont() { return Font.getDefaultFont(); }

    int contentHeight(Form f) {
        Font font = Font.getDefaultFont();
        int lines = 1;
        int avail = Math.max(f.getWidth() - 8, 1);
        String t = (label != null ? label + ": " : "") + (text == null ? "" : text);
        int w = font.stringWidth(t == null ? "" : t);
        lines = w <= avail ? 1 : (w + avail - 1) / avail;
        return lines * (font.getHeight() + 2) + 4;
    }

    void paintContent(Graphics g, int x, int y, int w, Form f) {
        Font font = Font.getDefaultFont();
        int lineH = font.getHeight() + 2;
        if (label != null && label.length() > 0) {
            g.setColor(appearanceMode == BUTTON ? 0x0000C0 : 0);
            drawWrapped(g, label + ":", x, y, w, lineH);
            y += lineH;
        }
        g.setColor(appearanceMode == HYPERLINK ? 0x0000C0 : 0x202020);
        drawWrapped(g, text == null ? "" : text, x, y, w, lineH);
    }

    static void drawWrapped(Graphics g, String s, int x, int y, int w, int lineH) {
        Font font = g.getFont();
        int cw = font.charWidth('x');
        int maxCols = Math.max(w / cw, 1);
        int pos = 0;
        while (pos < s.length()) {
            int end = Math.min(pos + maxCols, s.length());
            g.drawString(s.substring(pos, end), x, y, Graphics.LEFT | Graphics.TOP);
            pos = end;
            y += lineH;
        }
    }
}
