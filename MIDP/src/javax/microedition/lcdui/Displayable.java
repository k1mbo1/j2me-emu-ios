package javax.microedition.lcdui;

/**
 * Displayable 基类。Bridge C 层无视 Java 访问修饰符，
 * 直接按名调用子类的 paint/keyPressed/showNotify 等 protected 方法。
 */
public abstract class Displayable {

    String title;
    Ticker ticker;
    CommandListener listener;
    Command[] commands = new Command[4];
    int nCommands;

    public void setTitle(String t) { title = t; }
    public String getTitle() { return title; }

    public void setTicker(Ticker t) { ticker = t; }
    public Ticker getTicker() { return ticker; }

    public void addCommand(Command c) {
        if (c == null) return;
        for (int i = 0; i < nCommands; i++)
            if (commands[i] == c) return;
        if (nCommands == commands.length) {
            Command[] nc = new Command[nCommands * 2];
            System.arraycopy(commands, 0, nc, 0, nCommands);
            commands = nc;
        }
        commands[nCommands++] = c;
    }

    public void removeCommand(Command c) {
        for (int i = 0; i < nCommands; i++) {
            if (commands[i] == c) {
                int move = nCommands - i - 1;
                if (move > 0) System.arraycopy(commands, i + 1, commands, i, move);
                nCommands--;
                return;
            }
        }
    }

    public void setCommandListener(CommandListener l) { listener = l; }

    public int getWidth() { return Display.screenWidth; }
    public int getHeight() { return Display.screenHeight; }

    public boolean isShown() { return Display.current == this; }

    /* ---- 子类可覆写的生命周期（C 层按名直调） ---- */
    protected void showNotify() { }
    protected void hideNotify() { }
    protected void sizeChanged(int w, int h) { }

    /* ---- 事件入口（C 层按名直调，子类覆写） ---- */
    // Canvas: paint/keyPressed/keyRepeated/keyReleased/pointerPressed/pointerDragged/pointerReleased
    // Form 系: 内部处理

    void fireCommand(Command c) {
        CommandListener l = listener;
        if (l != null) l.commandAction(c, this);
    }
}
