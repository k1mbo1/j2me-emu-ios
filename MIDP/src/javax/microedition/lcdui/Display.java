package javax.microedition.lcdui;

import javax.microedition.midlet.MIDlet;

/**
 * Display：管理当前显示的 Displayable。
 * Bridge 帧循环约定：
 *   每帧调 Display.current 的 doFrame()（包内可见）
 *   Canvas 缓冲像素由 C 层直接读取渲染
 */
public final class Display {

    /** 当前显示内容（Bridge 读取） */
    static Displayable current;
    /** 屏幕逻辑尺寸（Bridge 启动时设置） */
    static int screenWidth = 240, screenHeight = 320;

    private static final Display instance = new Display();

    private Display() { }

    public static Display getDisplay(MIDlet m) { return instance; }

    public void setCurrent(Displayable d) {
        Displayable old = current;
        if (old == d) { if (d != null) nativeDisplayChanged(); return; }
        if (old != null) old.hideNotify();
        current = d;
        if (d != null) {
            int w = screenWidth, h = screenHeight;
            d.sizeChanged(w, h);
            d.showNotify();
        }
        nativeDisplayChanged();
    }

    public void setCurrent(Alert a, Displayable d) { setCurrent(d); }

    public Displayable getCurrent() { return current; }

    public boolean isShown(Displayable d) { return current == d; }

    public boolean vibrate(int ms) { return nativeVibrate(ms); }
    public boolean flashBacklight(int ms) { return false; }

    public static final int LIST_ELEMENT = 1, CHOICE_GROUP_ELEMENT = 2, POPUP = 3;

    /** Bridge 屏幕尺寸注入 */
    static void setScreenSize(int w, int h) {
        screenWidth = w;
        screenHeight = h;
        if (current != null) current.sizeChanged(w, h);
    }

    /* ---- natives（Bridge 注册） ---- */
    static native void nativeDisplayChanged();
    static native boolean nativeVibrate(int ms);
}
