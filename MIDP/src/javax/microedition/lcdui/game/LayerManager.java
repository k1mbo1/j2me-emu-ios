package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;

public class LayerManager {

    private Layer[] layers = new Layer[8];
    private int nLayers;
    private int viewX, viewY, viewW, viewH;

    public LayerManager() {
        viewW = Integer.MAX_VALUE;
        viewH = Integer.MAX_VALUE;
    }

    public void append(Layer l) { insert(l, 0); }

    public void insert(Layer l, int index) {
        if (l == null) throw new NullPointerException();
        /* 先移除已有 */
        remove(l);
        if (index < 0 || index > nLayers) throw new IndexOutOfBoundsException();
        if (nLayers == layers.length) {
            Layer[] nl = new Layer[nLayers * 2];
            System.arraycopy(layers, 0, nl, 0, nLayers);
            layers = nl;
        }
        int move = nLayers - index;
        if (move > 0) System.arraycopy(layers, index, layers, index + 1, move);
        layers[index] = l;
        nLayers++;
    }

    public Layer getLayerAt(int index) {
        if (index < 0 || index >= nLayers) throw new IndexOutOfBoundsException();
        return layers[index];
    }

    public int getSize() { return nLayers; }

    public void remove(Layer l) {
        if (l == null) return;
        for (int i = 0; i < nLayers; i++) {
            if (layers[i] == l) {
                int move = nLayers - i - 1;
                if (move > 0) System.arraycopy(layers, i + 1, layers, i, move);
                layers[--nLayers] = null;
                return;
            }
        }
    }

    public void setViewWindow(int x, int y, int width, int height) {
        if (width < 0 || height < 0) throw new IllegalArgumentException();
        viewX = x; viewY = y; viewW = width; viewH = height;
    }

    public void paint(Graphics g, int x, int y) {
        int cx = g.getClipX(), cy = g.getClipY();
        int cw = g.getClipWidth(), ch = g.getClipHeight();
        int dx = x - viewX, dy = y - viewY;
        g.translate(dx, dy);
        g.clipRect(viewX, viewY, viewW, viewH);
        /* 索引小 = 最上层 */
        for (int i = nLayers - 1; i >= 0; i--) {
            Layer l = layers[i];
            if (l == null || !l.isVisible()) continue;
            l.paint(g);
        }
        g.setClip(cx, cy, cw, ch);
        g.translate(-dx, -dy);
    }
}
