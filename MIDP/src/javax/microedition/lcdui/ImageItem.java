package javax.microedition.lcdui;

public class ImageItem extends Item {

    public static final int LAYOUT_DEFAULT = 0, LAYOUT_LEFT = 1, LAYOUT_RIGHT = 2,
            LAYOUT_CENTER = 3, LAYOUT_NEWLINE_BEFORE = 0x100, LAYOUT_NEWLINE_AFTER = 0x200;

    private Image image;
    private String altText;
    private int appearanceMode;

    public ImageItem(String label, Image img, int layout, String altText) {
        this(label, img, layout, altText, PLAIN);
    }

    public ImageItem(String label, Image img, int layout, String altText, int appearanceMode) {
        super(label);
        this.image = img;
        this.layout = layout;
        this.altText = altText;
        this.appearanceMode = appearanceMode;
    }

    public void setImage(Image img) { this.image = img; }
    public Image getImage() { return image; }
    public void setAltText(String text) { this.altText = text; }
    public String getAltText() { return altText; }
    public void setLayout(int layout) { this.layout = layout; }
    public int getLayout() { return layout; }
    public int getAppearanceMode() { return appearanceMode; }

    int contentHeight(Form f) {
        int h = image != null ? image.getHeight() + 4 : 16;
        if (label != null && label.length() > 0)
            h += Font.getDefaultFont().getHeight() + 2;
        return h;
    }

    void paintContent(Graphics g, int x, int y, int w, Form f) {
        Font font = Font.getDefaultFont();
        if (label != null && label.length() > 0) {
            g.setColor(0);
            g.drawString(label, x, y, Graphics.LEFT | Graphics.TOP);
            y += font.getHeight() + 2;
        }
        if (image != null) {
            g.drawImage(image, x + (w - image.getWidth()) / 2, y,
                    Graphics.LEFT | Graphics.TOP);
        } else if (altText != null) {
            g.setColor(0x808080);
            g.drawString(altText, x, y, Graphics.LEFT | Graphics.TOP);
        }
    }
}
