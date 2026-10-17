package com.nokia.mid.ui;

import javax.microedition.lcdui.Canvas;

/* FullCanvas（Nokia UI API）：全屏画布。
 * 国产 J2ME 游戏大量使用，此处提供兼容实现（构造即全屏）。 */
public abstract class FullCanvas extends Canvas {

    public static final int KEY_SOFTKEY1  = -6;
    public static final int KEY_SOFTKEY2  = -7;
    public static final int KEY_SOFTKEY3  = -8;
    public static final int KEY_SEND_KEY  = -10;
    public static final int KEY_END_KEY   = -11;
    public static final int KEY_CLEAR_KEY = -12;

    protected FullCanvas() {
        setFullScreenMode(true);
    }
}
