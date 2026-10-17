package javax.microedition.lcdui;

public abstract class Item {

    public static final int LAYOUT_DEFAULT = 0, LAYOUT_LEFT = 1, LAYOUT_RIGHT = 2,
            LAYOUT_CENTER = 3, LAYOUT_NEWLINE_BEFORE = 0x100,
            LAYOUT_NEWLINE_AFTER = 0x200, LAYOUT_VCENTER = 0x400,
            LAYOUT_TOP = 0x800, LAYOUT_BOTTOM = 0x1000,
            LAYOUT_VEXPAND = 0x2000, LAYOUT_VSHRINK = 0x4000;
    public static final int PLAIN = 0, HYPERLINK = 1, BUTTON = 2;

    String label;
    int layout = LAYOUT_DEFAULT;
    ItemCommandListener cmdListener;
    Command[] commands = new Command[2];
    int nCommands;
    Form owner;

    protected Item(String label) { this.label = label; }

    public void setLabel(String label) { this.label = label; }
    public String getLabel() { return label; }

    public void setLayout(int layout) { this.layout = layout; }
    public int getLayout() { return layout; }

    public void addCommand(Command cmd) {
        if (cmd == null) return;
        for (int i = 0; i < nCommands; i++) if (commands[i] == cmd) return;
        if (nCommands == commands.length) {
            Command[] nc = new Command[nCommands * 2];
            System.arraycopy(commands, 0, nc, 0, nCommands);
            commands = nc;
        }
        commands[nCommands++] = cmd;
    }

    public void removeCommand(Command cmd) {
        for (int i = 0; i < nCommands; i++)
            if (commands[i] == cmd) {
                int move = nCommands - i - 1;
                if (move > 0) System.arraycopy(commands, i + 1, commands, i, move);
                nCommands--;
                return;
            }
    }

    public void setDefaultCommand(Command cmd) {
        if (cmd != null) addCommand(cmd);
    }

    public void setItemCommandListener(ItemCommandListener l) { cmdListener = l; }

    /* 项高度（行数 * 行高，Screen 布局用） */
    int contentWidth(Form f) { return f.getWidth(); }
    int contentHeight(Form f) { return 16; }
    void paintContent(Graphics g, int x, int y, int w, Form f) { }

    void fireDefault() {
        if (cmdListener != null && nCommands > 0)
            cmdListener.commandAction(commands[0], this);
    }
}
