package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

/**
 * GameCanvas：双缓冲 + getKeyStates 轮询。
 * 帧缓冲即 Canvas.buffer；flushGraphics 把缓冲标记为可提交（Bridge 每帧读取）。
 */
public abstract class GameCanvas extends Canvas {

    public static final int UP_PRESSED = 1 << Canvas.UP;
    public static final int DOWN_PRESSED = 1 << Canvas.DOWN;
    public static final int LEFT_PRESSED = 1 << Canvas.LEFT;
    public static final int RIGHT_PRESSED = 1 << Canvas.RIGHT;
    public static final int FIRE_PRESSED = 1 << Canvas.FIRE;
    public static final int GAME_A_PRESSED = 1 << Canvas.GAME_A;
    public static final int GAME_B_PRESSED = 1 << Canvas.GAME_B;
    public static final int GAME_C_PRESSED = 1 << Canvas.GAME_C;
    public static final int GAME_D_PRESSED = 1 << Canvas.GAME_D;

    private int keyStates;
    private boolean suppressKeyEvents;

    protected GameCanvas(boolean suppressKeyEvents) {
        this.suppressKeyEvents = suppressKeyEvents;
    }

    public Graphics getGraphics() {
        return bg;
    }

    public void flushGraphics() {
        dirty = false;   /* 缓冲已是最新，Bridge 直接读取 */
        Canvas.nativeRepaint();
    }

    public void flushGraphics(int x, int y, int width, int height) {
        flushGraphics();
    }

    public int getKeyStates() {
        int s = keyStates;
        keyStates = 0;
        return s;
    }

    /* 键事件维护状态位；suppressKeyEvents 时不派发回调 */
    protected void keyPressed(int keyCode) {
        recordState(keyCode, true);
        if (!suppressKeyEvents) super.keyPressed(keyCode);
    }

    protected void keyReleased(int keyCode) {
        recordState(keyCode, false);
        if (!suppressKeyEvents) super.keyReleased(keyCode);
    }

    private void recordState(int keyCode, boolean on) {
        int bit = 0;
        switch (keyCode) {
            case KEY_NUM2: bit = UP_PRESSED; break;
            case KEY_NUM8: bit = DOWN_PRESSED; break;
            case KEY_NUM4: bit = LEFT_PRESSED; break;
            case KEY_NUM6: bit = RIGHT_PRESSED; break;
            case KEY_NUM5: bit = FIRE_PRESSED; break;
            case KEY_NUM1: bit = GAME_A_PRESSED; break;
            case KEY_NUM3: bit = GAME_B_PRESSED; break;
            case KEY_NUM7: bit = GAME_C_PRESSED; break;
            case KEY_NUM9: bit = GAME_D_PRESSED; break;
            default: return;
        }
        if (on) keyStates |= bit;
        else keyStates &= ~bit;
    }
}
