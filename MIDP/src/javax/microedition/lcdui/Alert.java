package javax.microedition.lcdui;

public class Alert extends Screen {

    public static final int FOREVER = -2;
    public static final Command DISMISS_COMMAND =
            new Command("OK", Command.OK, 0);

    private String text;
    private Image image;
    private int timeout = FOREVER;
    private Command dismiss = DISMISS_COMMAND;
    private boolean autoDismissed;

    public Alert(String title) { this(title, null, null, null); }

    public Alert(String title, String alertText, Image alertImage, AlertType alertType) {
        super(title);
        this.text = alertText;
        this.image = alertImage;
        addCommand(dismiss);
    }

    public void setString(String s) { text = s; repaintScreen(); }
    public String getString() { return text; }

    public void setImage(Image img) { image = img; repaintScreen(); }
    public Image getImage() { return image; }

    public void setTimeout(int t) { timeout = t; }
    public int getTimeout() { return timeout; }

    public void setType(AlertType type) { }
    public AlertType getType() { return null; }

    public void addCommand(Command cmd) {
        if (cmd == DISMISS_COMMAND) {
            if (dismiss != null && dismiss != DISMISS_COMMAND) super.removeCommand(dismiss);
            dismiss = cmd;
        }
        super.addCommand(cmd);
    }

    public void removeCommand(Command cmd) {
        if (cmd == dismiss) return;
        super.removeCommand(cmd);
    }

    public void setCommandListener(CommandListener l) {
        super.setCommandListener(l);
    }

    int paintContent(Graphics g, int x, int y, int w, int h) {
        Font font = Font.getDefaultFont();
        int cy = 8;
        if (image != null) {
            g.drawImage(image, w / 2, cy, Graphics.HCENTER | Graphics.TOP);
            cy += image.getHeight() + 8;
        }
        g.setColor(0);
        if (text != null) {
            StringItem.drawWrapped(g, text, x + 8, y + cy, w - 16, font.getHeight() + 2);
        }
        return cy + 40;
    }

    boolean contentKey(int keyCode) { return true; }

    void onSoftLeft() {
        if (nCommands > 0) fireCommand(commands[0]);
        else Display.getDisplay(null).setCurrent(null);
    }

    void onSoftRight() {
        Display.getDisplay(null).setCurrent(null);
    }

    String softLeftLabel() { return "OK"; }
    String softRightLabel() { return null; }
}
