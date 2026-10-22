package javax.microedition.lcdui;

public class Gauge extends Item {

    public static final int INDEFINITE = -1;
    public static final int CONTINUOUS_IDLE = 0, INCREMENTAL_IDLE = 1,
            CONTINUOUS_RUNNING = 2, INCREMENTAL_UPDATING = 3;

    private boolean interactive;
    private int maxValue, value;

    public Gauge(String label, boolean interactive, int maxValue, int initialValue) {
        super(label);
        if (maxValue == INDEFINITE && interactive)
            throw new IllegalArgumentException();
        if (!interactive && maxValue == INDEFINITE
                && (initialValue < CONTINUOUS_IDLE || initialValue > INCREMENTAL_UPDATING))
            throw new IllegalArgumentException();
        this.interactive = interactive;
        this.maxValue = maxValue;
        this.value = initialValue;
    }

    public void setValue(int value) {
        if (maxValue == INDEFINITE) throw new IllegalArgumentException();
        this.value = Math.max(0, Math.min(value, maxValue));
    }

    public int getValue() { return value; }

    public void setMaxValue(int maxValue) {
        if (maxValue <= 0 && maxValue != INDEFINITE) throw new IllegalArgumentException();
        this.maxValue = maxValue;
    }

    public int getMaxValue() { return maxValue; }
    public boolean isInteractive() { return interactive; }

    int contentHeight(Form f) {
        Font font = Font.getDefaultFont();
        return (label != null ? font.getHeight() + 2 : 0) + 20;
    }

    void paintContent(Graphics g, int x, int y, int w, Form f) {
        Font font = Font.getDefaultFont();
        if (label != null && label.length() > 0) {
            g.setColor(0);
            g.drawString(label, x, y, Graphics.LEFT | Graphics.TOP);
            y += font.getHeight() + 2;
        }
        g.setColor(0x808080);
        g.drawRect(x, y + 2, w - 8, 12);
        if (maxValue == INDEFINITE) {
            if (value == CONTINUOUS_RUNNING) {
                g.setColor(0x1050A0);
                g.fillRect(x + 2, y + 4, (w - 12) / 3, 8);
            }
        } else if (maxValue > 0) {
            int fw = (w - 12) * value / maxValue;
            g.setColor(0x1050A0);
            g.fillRect(x + 2, y + 4, fw, 8);
        }
        g.setColor(0);
    }
}
