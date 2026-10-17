package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class TiledLayer extends Layer {

    private final int cols, rows, tileW, tileH;
    private Image image;
    private int[] tiles;           /* 0=空，>0=静态 tile，<0=动画帧序号 */
    private int nStaticTiles;
    private int[][] animFrames;    /* 动画 tile 序列 */
    private int[] animIdx;
    private int nAnim;

    public TiledLayer(int columns, int rows, Image image, int tileWidth, int tileHeight) {
        super(columns <= 0 || tileWidth <= 0 ? 0 : columns * tileWidth,
               rows <= 0 || tileHeight <= 0 ? 0 : rows * tileHeight);
        if (columns < 1 || rows < 1 || image == null
                || image.getWidth() % tileWidth != 0 || image.getHeight() % tileHeight != 0)
            throw new IllegalArgumentException();
        this.cols = columns;
        this.rows = rows;
        this.tileW = tileWidth;
        this.tileH = tileHeight;
        this.image = image;
        this.tiles = new int[columns * rows];
        this.nStaticTiles = (image.getWidth() / tileWidth) * (image.getHeight() / tileHeight);
        this.animFrames = new int[4][];
        this.animIdx = new int[4];
    }

    public int createAnimatedTile(int staticTileIndex) {
        if (staticTileIndex < 0 || staticTileIndex > nStaticTiles)
            throw new IndexOutOfBoundsException();
        if (nAnim == animFrames.length) {
            int[][] nf = new int[nAnim * 2][];
            System.arraycopy(animFrames, 0, nf, 0, nAnim);
            animFrames = nf;
            int[] ni = new int[nAnim * 2];
            System.arraycopy(animIdx, 0, ni, 0, nAnim);
            animIdx = ni;
        }
        animFrames[nAnim] = new int[] { staticTileIndex };
        nAnim++;
        return -nAnim;   /* 动画 tile 编号从 -1 开始 */
    }

    public void setAnimatedTile(int animatedTileIndex, int staticTileIndex) {
        int a = -animatedTileIndex - 1;
        if (a < 0 || a >= nAnim || staticTileIndex < 0 || staticTileIndex > nStaticTiles)
            throw new IndexOutOfBoundsException();
        animFrames[a][animIdx[a]] = staticTileIndex;
    }

    public int getAnimatedTile(int animatedTileIndex) {
        int a = -animatedTileIndex - 1;
        if (a < 0 || a >= nAnim) throw new IndexOutOfBoundsException();
        return animFrames[a][animIdx[a]];
    }

    public void fillCells(int col, int row, int numCols, int numRows, int tileIndex) {
        checkCells(col, row, numCols, numRows);
        if (tileIndex > nStaticTiles || tileIndex < -nAnim)
            throw new IndexOutOfBoundsException();
        for (int r = row; r < row + numRows; r++)
            for (int c = col; c < col + numCols; c++)
                tiles[r * cols + c] = tileIndex;
    }

    public void setCell(int col, int row, int tileIndex) {
        checkCells(col, row, 1, 1);
        if (tileIndex > nStaticTiles || tileIndex < -nAnim)
            throw new IndexOutOfBoundsException();
        tiles[row * cols + col] = tileIndex;
    }

    public int getCell(int col, int row) {
        checkCells(col, row, 1, 1);
        int t = tiles[row * cols + col];
        if (t < 0) return animFrames[-t - 1][animIdx[-t - 1]];
        return t;
    }

    public final int getCellWidth() { return tileW; }
    public final int getCellHeight() { return tileH; }
    public final int getColumns() { return cols; }
    public final int getRows() { return rows; }

    public void setStaticTileSet(Image image, int tileWidth, int tileHeight) {
        if (image == null || image.getWidth() % tileWidth != 0
                || image.getHeight() % tileHeight != 0)
            throw new IllegalArgumentException();
        int n = (image.getWidth() / tileWidth) * (image.getHeight() / tileHeight);
        if (n < nStaticTiles) {
            for (int i = 0; i < tiles.length; i++)
                if (tiles[i] > n) tiles[i] = 0;
        }
        this.image = image;
        this.nStaticTiles = n;
    }

    private void checkCells(int col, int row, int nc, int nr) {
        if (col < 0 || row < 0 || nc < 0 || nr < 0
                || col + nc > cols || row + nr > rows)
            throw new IndexOutOfBoundsException();
    }

    /* 碰撞：指定矩形（世界坐标）与任意非空 cell 相交 */
    boolean collidesRect(int wx, int wy, int ww, int wh) {
        int c1 = Math.max(0, (wx - x) / tileW);
        int r1 = Math.max(0, (wy - y) / tileH);
        int c2 = Math.min(cols - 1, (wx + ww - 1 - x) / tileW);
        int r2 = Math.min(rows - 1, (wy + wh - 1 - y) / tileH);
        for (int r = r1; r <= r2; r++)
            for (int c = c1; c <= c2; c++)
                if (tiles[r * cols + c] != 0) return true;
        return false;
    }

    public final void paint(Graphics g) {
        if (!visible) return;
        int tilesPerRow = image.getWidth() / tileW;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                int t = tiles[r * cols + c];
                if (t == 0) continue;
                if (t < 0) t = animFrames[-t - 1][animIdx[-t - 1]];
                if (t == 0) continue;
                int col = (t - 1) % tilesPerRow, row = (t - 1) / tilesPerRow;
                g.drawRegion(image, col * tileW, row * tileH, tileW, tileH,
                        Sprite.TRANS_NONE, x + c * tileW, y + r * tileH,
                        Graphics.TOP | Graphics.LEFT);
            }
        }
    }
}
