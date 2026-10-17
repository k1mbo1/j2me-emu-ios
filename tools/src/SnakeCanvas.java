import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.rms.RecordStore;

/** 经典贪吃蛇：方向键/数字键 2468 控制，5 暂停，撞墙或撞己身结束 */
public class SnakeCanvas extends Canvas implements Runnable {

    private static final int CELL = 10;
    private static final int COLS = 22;
    private static final int ROWS = 26;
    private static final int OFF_X = 10;
    private static final int OFF_Y = 40;

    private int[] snakeX = new int[COLS * ROWS];
    private int[] snakeY = new int[COLS * ROWS];
    private int len;
    private int dirX, dirY;      // 当前方向
    private int nextDirX, nextDirY;
    private int foodX, foodY;
    private int score;
    private int best;
    private boolean dead;
    private boolean paused;
    private Thread thread;
    private final Font font = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM);
    private final Font small = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);

    public SnakeCanvas() {
        setFullScreenMode(true);
        best = loadBest();
        reset();
    }

    private void reset() {
        len = 4;
        for (int i = 0; i < len; i++) {
            snakeX[i] = COLS / 2 - i;
            snakeY[i] = ROWS / 2;
        }
        dirX = 1; dirY = 0;
        nextDirX = 1; nextDirY = 0;
        score = 0;
        dead = false;
        paused = false;
        placeFood();
    }

    private void placeFood() {
        while (true) {
            int x = (int) (Math.random() * COLS);
            int y = (int) (Math.random() * ROWS);
            boolean hit = false;
            for (int i = 0; i < len; i++)
                if (snakeX[i] == x && snakeY[i] == y) { hit = true; break; }
            if (!hit) { foodX = x; foodY = y; return; }
        }
    }

    public void start() {
        if (thread == null) {
            thread = new Thread(this);
            thread.start();
        }
    }

    public void pause() {
        thread = null;
    }

    public void run() {
        Thread me = Thread.currentThread();
        while (thread == me) {
            long t0 = System.currentTimeMillis();
            if (!paused && !dead) step();
            repaint();
            serviceRepaints();
            long dt = System.currentTimeMillis() - t0;
            int speed = Math.max(70, 180 - score * 2);
            try { Thread.sleep(Math.max(10, speed - dt)); } catch (InterruptedException e) { break; }
        }
    }

    private void step() {
        if (nextDirX != -dirX || nextDirY != -dirY) {
            dirX = nextDirX; dirY = nextDirY;
        }
        int hx = snakeX[0] + dirX;
        int hy = snakeY[0] + dirY;
        if (hx < 0 || hx >= COLS || hy < 0 || hy >= ROWS) { die(); return; }
        for (int i = 0; i < len; i++)
            if (snakeX[i] == hx && snakeY[i] == hy) { die(); return; }
        if (hx == foodX && hy == foodY) {
            // 长大：尾部保留
            for (int i = len; i > 0; i--) {
                snakeX[i] = snakeX[i - 1];
                snakeY[i] = snakeY[i - 1];
            }
            snakeX[0] = hx; snakeY[0] = hy;
            len++;
            score += 10;
            placeFood();
        } else {
            for (int i = len - 1; i > 0; i--) {
                snakeX[i] = snakeX[i - 1];
                snakeY[i] = snakeY[i - 1];
            }
            snakeX[0] = hx; snakeY[0] = hy;
        }
    }

    private void die() {
        dead = true;
        if (score > best) { best = score; saveBest(); }
    }

    protected void keyPressed(int keyCode) {
        int action = getGameAction(keyCode);
        switch (action) {
            case UP: case GAME_A: if (dirY != 1) { nextDirX = 0; nextDirY = -1; } break;
            case DOWN: case GAME_B: if (dirY != -1) { nextDirX = 0; nextDirY = 1; } break;
            case LEFT: case GAME_C: if (dirX != 1) { nextDirX = -1; nextDirY = 0; } break;
            case RIGHT: case GAME_D: if (dirX != -1) { nextDirX = 1; nextDirY = 0; } break;
            case FIRE:
                if (dead) reset();
                else paused = !paused;
                break;
            default:
                if (keyCode == '2') { if (dirY != 1) { nextDirX = 0; nextDirY = -1; } }
                else if (keyCode == '8') { if (dirY != -1) { nextDirX = 0; nextDirY = 1; } }
                else if (keyCode == '4') { if (dirX != 1) { nextDirX = -1; nextDirY = 0; } }
                else if (keyCode == '6') { if (dirX != -1) { nextDirX = 1; nextDirY = 0; } }
                else if (keyCode == '5') { if (dead) reset(); else paused = !paused; }
                break;
        }
    }

    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight();
        // 背景
        g.setColor(0x0E1420);
        g.fillRect(0, 0, w, h);
        // 场地
        g.setColor(0x16202F);
        g.fillRect(OFF_X - 2, OFF_Y - 2, COLS * CELL + 4, ROWS * CELL + 4);
        g.setColor(0x2E6BE6);
        g.drawRect(OFF_X - 2, OFF_Y - 2, COLS * CELL + 4, ROWS * CELL + 4);
        // 食物（红方块）
        g.setColor(0xFF5252);
        g.fillRoundRect(OFF_X + foodX * CELL + 1, OFF_Y + foodY * CELL + 1, CELL - 2, CELL - 2, 3, 3);
        // 蛇身
        for (int i = len - 1; i >= 0; i--) {
            if (i == 0) g.setColor(0x7FD15A);
            else g.setColor(0x4CAF50);
            g.fillRoundRect(OFF_X + snakeX[i] * CELL + 1, OFF_Y + snakeY[i] * CELL + 1,
                    CELL - 2, CELL - 2, 3, 3);
        }
        // 顶栏
        g.setColor(0xF2F5FA);
        g.setFont(font);
        g.drawString("贪吃蛇", OFF_X, 12, Graphics.LEFT | Graphics.TOP);
        g.setFont(small);
        g.setColor(0x9AA5B5);
        g.drawString("分数 " + score, w - OFF_X, 8, Graphics.RIGHT | Graphics.TOP);
        g.drawString("最高 " + best, w - OFF_X, 24, Graphics.RIGHT | Graphics.TOP);
        // 状态提示
        if (paused && !dead) {
            centerText(g, "已暂停 - 按 5/FIRE 继续", h / 2);
        } else if (dead) {
            g.setColor(0xAA000000);
            g.fillRect(0, h / 2 - 34, w, 68);
            centerText(g, "游戏结束  分数 " + score, h / 2 - 12);
            centerText(g, "按 5/FIRE 重新开始", h / 2 + 12);
        }
    }

    private void centerText(Graphics g, String s, int y) {
        g.setColor(0xF2F5FA);
        g.setFont(font);
        g.drawString(s, getWidth() / 2, y, Graphics.HCENTER | Graphics.TOP);
    }

    // ---------- RMS 最高分 ----------

    private int loadBest() {
        try {
            RecordStore rs = RecordStore.openRecordStore("snake", true);
            try {
                if (rs.getNumRecords() > 0) {
                    byte[] d = rs.getRecord(1);
                    if (d != null && d.length > 0)
                        return Integer.parseInt(new String(d, "UTF-8").trim());
                }
            } finally { rs.closeRecordStore(); }
        } catch (Exception e) { return 0; }
        return 0;
    }

    public void saveBest() {
        try {
            byte[] d = String.valueOf(best).getBytes("UTF-8");
            RecordStore rs = RecordStore.openRecordStore("snake", true);
            try {
                if (rs.getNumRecords() > 0) rs.setRecord(1, d, 0, d.length);
                else rs.addRecord(d, 0, d.length);
            } finally { rs.closeRecordStore(); }
        } catch (Exception ignore) {}
    }
}
