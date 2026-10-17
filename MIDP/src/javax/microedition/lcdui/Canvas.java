package javax.microedition.lcdui;

/**
 * Canvas：游戏/自绘核心。
 * 像素缓冲 int[] ARGB，Bridge 帧循环约定：
 *   1. Display.current 为 Canvas 时每帧调 doFrame()
 *   2. doFrame 内 dirty 则 paint(bg)（bg 目标 = buffer）
 *   3. Bridge 直接读 buffer 像素上传纹理
 * 键事件：Bridge 经 vmCallSpecialByName 直调 keyPressed 等 protected 方法
 */
public abstract class Canvas extends Displayable {

    /* 游戏动作常量 */
    public static final int UP = 1, LEFT = 2, RIGHT = 5, DOWN = 6, FIRE = 8,
            GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    /* 键码 */
    public static final int KEY_NUM0 = 48, KEY_NUM1 = 49, KEY_NUM2 = 50, KEY_NUM3 = 51,
            KEY_NUM4 = 52, KEY_NUM5 = 53, KEY_NUM6 = 54, KEY_NUM7 = 55,
            KEY_NUM8 = 56, KEY_NUM9 = 57, KEY_STAR = 42, KEY_POUND = 35;

    /* 帧缓冲（Bridge + game 子包读取） */
    public int[] buffer;
    public int bufW, bufH;
    public Graphics bg;
    public boolean dirty = true;
    boolean fullScreen = true;

    protected Canvas() {
        bufW = Display.screenWidth;
        bufH = Display.screenHeight;
        buffer = new int[bufW * bufH];
        bg = new Graphics(buffer, bufW, bufW, bufH);
    }

    /* ---- 子类 API ---- */
    protected abstract void paint(Graphics g);

    protected void keyPressed(int keyCode) { }
    protected void keyRepeated(int keyCode) { }
    protected void keyReleased(int keyCode) { }
    protected void pointerPressed(int x, int y) { }
    protected void pointerDragged(int x, int y) { }
    protected void pointerReleased(int x, int y) { }

    public int getGameAction(int keyCode) {
        switch (keyCode) {
            case UP: case KEY_NUM2: return UP;
            case DOWN: case KEY_NUM8: return DOWN;
            case LEFT: case KEY_NUM4: return LEFT;
            case RIGHT: case KEY_NUM6: return RIGHT;
            case KEY_NUM5: return FIRE;
            case KEY_NUM1: return GAME_A;
            case KEY_NUM3: return GAME_B;
            case KEY_NUM7: return GAME_C;
            case KEY_NUM9: return GAME_D;
            case KEY_STAR: return GAME_A;
            case KEY_POUND: return GAME_B;
            default: return 0;
        }
    }

    public int getKeyCode(int gameAction) {
        switch (gameAction) {
            case UP: return KEY_NUM2;
            case DOWN: return KEY_NUM8;
            case LEFT: return KEY_NUM4;
            case RIGHT: return KEY_NUM6;
            case FIRE: return KEY_NUM5;
            case GAME_A: return KEY_NUM1;
            case GAME_B: return KEY_NUM3;
            case GAME_C: return KEY_NUM7;
            case GAME_D: return KEY_NUM9;
            default: return 0;
        }
    }

    public String getKeyName(int keyCode) {
        switch (keyCode) {
            case KEY_NUM0: return "0";
            case KEY_NUM1: return "1";
            case KEY_NUM2: return "2";
            case KEY_NUM3: return "3";
            case KEY_NUM4: return "4";
            case KEY_NUM5: return "5";
            case KEY_NUM6: return "6";
            case KEY_NUM7: return "7";
            case KEY_NUM8: return "8";
            case KEY_NUM9: return "9";
            case KEY_STAR: return "*";
            case KEY_POUND: return "#";
            default: return "KEY_" + keyCode;
        }
    }

    public void setFullScreenMode(boolean on) {
        fullScreen = on;
    }

    public boolean isDoubleBuffered() { return true; }

    public boolean hasPointerEvents() { return true; }
    public boolean hasPointerMotionEvents() { return true; }
    public boolean hasRepeatEvents() { return true; }

    /* ---- 重绘 ---- */
    public final void repaint() {
        dirty = true;
        nativeRepaint();
    }

    public final void repaint(int x, int y, int w, int h) {
        dirty = true;   /* v1.0.0 全量重绘（区域优化后续版本） */
        nativeRepaint();
    }

    public final void serviceRepaint() {
        if (dirty) doFrame();
    }

    /* ---- 帧驱动（Bridge 每帧调用，包内可见） ---- */
    void doFrame() {
        if (dirty) {
            dirty = false;
            paint(bg);
        }
    }

    /* 尺寸变化时重建缓冲 */
    protected void sizeChanged(int w, int h) {
        if (w <= 0 || h <= 0 || (w == bufW && h == bufH)) return;
        bufW = w; bufH = h;
        buffer = new int[bufW * bufH];
        bg = new Graphics(buffer, bufW, bufW, bufH);
        dirty = true;
    }

    public static native void nativeRepaint();
}
