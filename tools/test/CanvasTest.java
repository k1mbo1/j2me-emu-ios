import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

/**
 * CanvasTest：headless 渲染管线端到端验证。
 * 验证：MIDlet 生命周期 / Canvas paint / Graphics 图形/文本 / RMS 读写 / 键事件。
 * 预期输出（stdout）：
 *   CT: startApp
 *   CT: rms-put 5
 *   CT: rms-get 5
 *   CT: key 50
 *   CT: paint #1 .. #N（帧数）
 *   CT: destroy
 */
public class CanvasTest extends MIDlet implements CommandListener {

    private TestCanvas canvas;
    private int frameCount;
    private boolean rmsOk;
    private int lastKey = -1;

    public CanvasTest() { }

    protected void startApp() {
        System.out.println("CT: startApp");
        /* RMS 写读 */
        try {
            RecordStore rs = RecordStore.openRecordStore("ct_test", true);
            byte[] d = { 1, 2, 3, 4, 5 };
            rs.addRecord(d, 0, d.length);
            byte[] back = rs.getRecord(1);
            System.out.println("CT: rms-put " + d.length);
            System.out.println("CT: rms-get " + (back == null ? -1 : back.length));
            rs.closeRecordStore();
            rmsOk = true;
        } catch (Exception e) {
            System.out.println("CT: rms-fail " + e);
        }
        canvas = new TestCanvas();
        Display.getDisplay(this).setCurrent(canvas);
    }

    protected void pauseApp() { }

    protected void destroyApp(boolean unconditional) {
        System.out.println("CT: destroy painted=" + frameCount + " rms=" + rmsOk + " key=" + lastKey);
    }

    public void commandAction(Command c, javax.microedition.lcdui.Displayable d) { }

    class TestCanvas extends Canvas {
        protected void paint(Graphics g) {
            frameCount++;
            System.out.println("CT: paint #" + frameCount);
            int w = getWidth(), h = getHeight();
            /* 背景 */
            g.setColor(0x204060);
            g.fillRect(0, 0, w, h);
            /* 图形 */
            g.setColor(0xFFFFFF);
            g.drawLine(10, 10, w - 10, 10);
            g.drawRect(10, 20, 50, 30);
            g.setColor(0xFFCC00);
            g.fillRect(12, 22, 47, 27);
            g.setColor(0xFF0000);
            g.fillArc(80, 20, 40, 40, 0, 360);
            g.setColor(0x00CC00);
            g.drawLine(10, 60, 60, 110);
            /* 文本 */
            g.setColor(0xFFFFFF);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM));
            g.drawString("Hello J2ME!", 10, 120, Graphics.TOP | Graphics.LEFT);
            g.setFont(Font.getDefaultFont());
            g.drawString("canvas " + w + "x" + h + " rms=" + rmsOk, 10, 140, Graphics.TOP | Graphics.LEFT);
            /* 不可变 Image（程序生成 RGB） */
            int[] px = new int[16 * 16];
            for (int i = 0; i < 16 * 16; i++) px[i] = 0xFF0000FF;
            Image img = Image.createRGBImage(px, 16, 16, false);
            g.drawImage(img, 130, 120, Graphics.TOP | Graphics.LEFT);
        }

        protected void keyPressed(int keyCode) {
            lastKey = keyCode;
            System.out.println("CT: key " + keyCode);
            repaint();
        }
    }
}
