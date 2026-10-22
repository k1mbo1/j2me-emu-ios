package javax.microedition.lcdui;

/**
 * Screen 基类：Form/TextBox/List/Alert 共用自绘框架。
 * 标题栏 + 内容区 + 软键栏（-6 左软键 / -7 右软键，J2ME 惯例键码）。
 * Bridge 帧循环调 doFrame()，键事件直调 keyPressed/pointerPressed。
 */
public abstract class Screen extends Displayable {

    static final int SOFT_LEFT = -6, SOFT_RIGHT = -7;

    int[] buffer;
    int bufW, bufH;
    Graphics bg;
    boolean dirty = true;
    int scrollY;

    protected Screen(String title) {
        this.title = title;
        bufW = Display.screenWidth;
        bufH = Display.screenHeight;
        buffer = new int[bufW * bufH];
        bg = new Graphics(buffer, bufW, bufW, bufH);
    }

    /* 子类：内容区绘制（返回内容总高） */
    abstract int paintContent(Graphics g, int x, int y, int w, int h);
    /* 子类：内容键处理（导航键/字符），返回是否处理 */
    abstract boolean contentKey(int keyCode);

    int contentHeight() { return bufH - 46; }

    void doFrame() {
        if (!dirty) return;
        dirty = false;
        Graphics g = bg;
        g.setColor(0xE0E0E0);
        g.fillRect(0, 0, bufW, bufH);
        /* 标题栏 */
        g.setColor(0x1050A0);
        g.fillRect(0, 0, bufW, 22);
        g.setColor(0xFFFFFF);
        if (title != null)
            g.drawString(title, 4, 4, Graphics.LEFT | Graphics.TOP);
        /* 内容 */
        int top = 24, bottom = bufH - 22;
        int ch = paintContent(g, 0, top - scrollY, bufW, bottom - top);
        /* 滚动修正 */
        if (ch > bottom - top) {
            if (scrollY > ch - (bottom - top)) scrollY = ch - (bottom - top);
            if (scrollY < 0) scrollY = 0;
        } else scrollY = 0;
        /* 软键栏 */
        g.setColor(0xB0B0B0);
        g.fillRect(0, bottom, bufW, 22);
        g.setColor(0);
        String left = softLeftLabel();
        String right = softRightLabel();
        if (left != null) g.drawString(left, 4, bottom + 4, Graphics.LEFT | Graphics.TOP);
        if (right != null) g.drawString(right, bufW - 4, bottom + 4, Graphics.RIGHT | Graphics.TOP);
    }

    String softLeftLabel() { return null; }
    String softRightLabel() { return null; }

    void repaintScreen() {
        dirty = true;
        if (isShown()) nativeRepaint();
    }

    static native void nativeRepaint();

    /* ---- 键处理（C 层直调） ---- */
    protected void keyPressed(int keyCode) {
        if (keyCode == SOFT_LEFT) { onSoftLeft(); return; }
        if (keyCode == SOFT_RIGHT) { onSoftRight(); return; }
        if (keyCode == Canvas.KEY_NUM2 || keyCode == Canvas.UP) { scrollY -= 24; }
        else if (keyCode == Canvas.KEY_NUM8 || keyCode == Canvas.DOWN) { scrollY += 24; }
        boolean handled = contentKey(keyCode);
        repaintScreen();
        if (!handled && (keyCode == Canvas.KEY_NUM5 || keyCode == Canvas.FIRE)) onFire();
    }

    protected void keyRepeated(int keyCode) { keyPressed(keyCode); }
    protected void keyReleased(int keyCode) { }

    void onSoftLeft() { }
    void onSoftRight() { }
    void onFire() { }

    protected void pointerPressed(int x, int y) {
        int bottom = bufH - 22;
        if (y >= bottom) {
            if (x < bufW / 2) onSoftLeft(); else onSoftRight();
        }
        repaintScreen();
    }

    protected void sizeChanged(int w, int h) {
        if (w <= 0 || h <= 0 || (w == bufW && h == bufH)) return;
        bufW = w; bufH = h;
        buffer = new int[bufW * bufH];
        bg = new Graphics(buffer, bufW, bufW, bufH);
        dirty = true;
    }
}
