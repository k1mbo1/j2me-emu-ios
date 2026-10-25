/* CjkTest.java - e2e 验证游戏：中文渲染方向 + 游戏线程渲染时序（闪烁）
 * 模拟 S40 游戏标准循环：线程里 repaint()+serviceRepaints()+sleep(30)，
 * paint 开头整屏清黑再重绘。若 paint 跑在游戏线程（bug），主帧循环会拷贝到
 * 清屏后的半成品帧 -> 截图出现黑帧/缺格；修复后截图恒为完整棋盘格+中文。 */
import javax.microedition.lcdui.*;
import javax.microedition.midlet.MIDlet;
import com.nokia.mid.ui.FullCanvas;

public class CjkTest extends MIDlet implements Runnable {
    private FullCanvas cv;
    private boolean started = false;

    public void startApp() {
        if (started) return;
        started = true;
        cv = new FullCanvas() {
            protected void paint(Graphics g) {
                int w = getWidth(), h = getHeight();
                g.setColor(0, 0, 0);
                g.fillRect(0, 0, w, h);
                /* 棋盘格：黑帧检测特征（蓝格必须每帧都在） */
                for (int y = 0; y < h; y += 16) {
                    for (int x = 0; x < w; x += 16) {
                        if (((x / 16 + y / 16) & 1) == 0) {
                            g.setColor(40, 90, 170);
                            g.fillRect(x, y, 16, 16);
                        }
                    }
                }
                /* 绘制负载：模拟真实游戏一帧的耗时（撑长 paint 窗口期），垫底 */
                g.setColor(255, 255, 255);
                for (int i = 0; i < 60; i++) {
                    g.drawLine(0, i * 4, w, i * 4);
                }
                /* 黄色中文：字形方向人工核验特征（最后画，保证完整可读） */
                g.setColor(255, 255, 0);
                g.drawString("中文测试：歪歪歪传战国风云", 8, 24, 0);
                g.drawString("载入游戏，请等待。", 8, 44, 0);
            }
        };
        Display.getDisplay(this).setCurrent(cv);
        new Thread(this).start();
    }

    public void run() {
        while (true) {
            cv.repaint();
            cv.serviceRepaints();
            try { Thread.sleep(30); } catch (InterruptedException e) { return; }
        }
    }

    public void pauseApp() { }
    public void destroyApp(boolean unconditional) { }
}
