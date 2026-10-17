package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;

public abstract class Layer {

    int x, y, width, height;
    boolean visible = true;

    Layer(int w, int h) { width = w; height = h; }

    public void setPosition(int x, int y) { this.x = x; this.y = y; }
    public void move(int dx, int dy) { x += dx; y += dy; }
    public int getX() { return x; }
    public int getY() { return y; }
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    public void setVisible(boolean visible) { this.visible = visible; }
    public boolean isVisible() { return visible; }

    public abstract void paint(Graphics g);
}
