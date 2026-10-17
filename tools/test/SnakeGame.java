import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Font;
import javax.microedition.midlet.MIDlet;

/**
 * SnakeGame：按键驱动贪吃蛇（headless 可稳定验证）。
 * 每次方向键走一步；吃食物加分长节；撞墙/自身 GAME OVER 可按键重开。
 * destroyApp 打印 "SG: destroy score=N len=M gameOver=B"
 */
public class SnakeGame extends MIDlet {

    private GameCanvasEx canvas;
    private int score;
    private int snakeLen;
    private boolean gameOver;

    public SnakeGame() { }

    protected void startApp() {
        System.out.println("SG: startApp");
        canvas = new GameCanvasEx();
        Display.getDisplay(this).setCurrent(canvas);
    }

    protected void pauseApp() { }

    protected void destroyApp(boolean unconditional) {
        System.out.println("SG: destroy score=" + score + " len=" + snakeLen
                + " gameOver=" + gameOver);
    }

    class GameCanvasEx extends Canvas {
        /* 15x20 格，每格 16px (240x320) */
        static final int COLS = 15, ROWS = 19, CELL = 16, TOP = 16;
        int[] xs = new int[COLS * ROWS];
        int[] ys = new int[COLS * ROWS];
        int len = 3;
        int dirX = 1, dirY = 0;
        int pendingX = 1, pendingY = 0;
        int foodX, foodY;
        boolean over;

        GameCanvasEx() {
            for (int i = 0; i < len; i++) {
                xs[i] = 5 - i;
                ys[i] = 9;
            }
            /* 首颗食物固定在右行路径，headless 确定性验证（keys 666 必吃到） */
            foodX = 8;
            foodY = 9;
        }

        void placeFood() {
            /* 简单 LCG，避免与蛇身重叠 */
            long seed = System.currentTimeMillis() + 12345;
            for (int tries = 0; tries < 200; tries++) {
                seed = seed * 1103515245L + 12345L;
                foodX = (int) ((seed >> 16) & 0x7FFFFFFF) % COLS;
                seed = seed * 1103515245L + 12345L;
                foodY = (int) ((seed >> 16) & 0x7FFFFFFF) % ROWS;
                if (foodX < 0) foodX += COLS;
                if (foodY < 0) foodY += ROWS;
                boolean hit = false;
                for (int i = 0; i < len; i++)
                    if (xs[i] == foodX && ys[i] == foodY) { hit = true; break; }
                if (!hit) return;
            }
            foodX = 0; foodY = 0;
        }

        void step() {
            dirX = pendingX;
            dirY = pendingY;
            int nx = xs[0] + dirX, ny = ys[0] + dirY;
            if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { over = true; return; }
            for (int i = 0; i < len; i++)
                if (xs[i] == nx && ys[i] == ny) { over = true; return; }
            boolean grow = (nx == foodX && ny == foodY);
            if (grow) {
                len++;
                score++;
                placeFood();
            }
            /* 移动（尾部补位） */
            for (int i = len - 1; i > 0; i--) {
                xs[i] = xs[i - 1];
                ys[i] = ys[i - 1];
            }
            xs[0] = nx;
            ys[0] = ny;
        }

        protected void keyPressed(int keyCode) {
            int act = getGameAction(keyCode);
            if (over) {
                if (act == FIRE) reset();
                return;
            }
            switch (act) {
                case UP:    if (dirY == 0) { pendingX = 0; pendingY = -1; } break;
                case DOWN:  if (dirY == 0) { pendingX = 0; pendingY = 1; } break;
                case LEFT:  if (dirX == 0) { pendingX = -1; pendingY = 0; } break;
                case RIGHT: if (dirX == 0) { pendingX = 1; pendingY = 0; } break;
                case FIRE:  step(); break;
                default: return;
            }
            if (act != FIRE) step();
            repaint();
        }

        void reset() {
            len = 3;
            for (int i = 0; i < len; i++) { xs[i] = 5 - i; ys[i] = 9; }
            dirX = 1; dirY = 0; pendingX = 1; pendingY = 0;
            score = 0;
            over = false;
            placeFood();
            repaint();
        }

        protected void paint(Graphics g) {
            score = score;  /* 供 destroyApp 读取 */
            snakeLen = len;
            gameOver = over;
            int w = getWidth(), h = getHeight();
            g.setColor(0x101820);
            g.fillRect(0, 0, w, h);
            /* 顶栏 */
            g.setColor(0x000000);
            g.fillRect(0, 0, w, TOP);
            g.setColor(0xFFFFFF);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM));
            g.drawString("SNAKE  score " + score, 4, 2, Graphics.TOP | Graphics.LEFT);
            /* 网格区背景 */
            g.setColor(0x182830);
            g.fillRect(0, TOP, w, h - TOP);
            /* 食物 */
            g.setColor(0xFF2222);
            g.fillRect(foodX * CELL + 2, TOP + foodY * CELL + 2, CELL - 4, CELL - 4);
            /* 蛇 */
            for (int i = 0; i < len; i++) {
                int c = 0x22CC44;
                if (i == 0) c = 0x66FF88;
                g.setColor(c);
                g.fillRect(xs[i] * CELL + 1, TOP + ys[i] * CELL + 1, CELL - 2, CELL - 2);
            }
            if (over) {
                g.setColor(0xFFFF00);
                g.drawString("GAME OVER - press 5", 10, h / 2, Graphics.TOP | Graphics.LEFT);
            }
        }
    }
}
