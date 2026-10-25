import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.rms.RecordStore;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;

/** RMS 存读档回归：复刻《歪歪歪传》new()/for() 的完整链路。
 *  首次运行写档 -> 画面蓝色块（SAVE_OK）；再次运行读回比对 -> 绿色块（LOAD_OK）；
 *  任何异常 -> 红色块（FAIL）。e2e 用截图像素断言判定。 */
public class RmsTest extends MIDlet {
    static final String STORE = "war";
    static int result = 0;   /* 1=SAVE_OK 2=LOAD_OK 3=FAIL */

    public void startApp() {
        try {
            RecordStore rs = RecordStore.openRecordStore(STORE, true);
            int n = rs.getNumRecords();
            rs.closeRecordStore();
            if (n == 0) {
                /* ---- 写档（复刻 new()：BAOS+DOS 序列化 -> delete -> open -> addRecord） ---- */
                ByteArrayOutputStream bos = new ByteArrayOutputStream();
                DataOutputStream dos = new DataOutputStream(bos);
                dos.writeInt(0x1010F0F0);
                dos.writeInt(77);
                dos.writeInt(88);
                dos.writeBoolean(true);
                dos.writeUTF("歪歪歪传战国风云存档测试");
                dos.close();
                byte[] data = bos.toByteArray();
                try { RecordStore.deleteRecordStore(STORE); } catch (Exception e) { }
                RecordStore w = RecordStore.openRecordStore(STORE, true);
                w.addRecord(data, 0, data.length);
                w.closeRecordStore();
                result = 1;
            } else {
                /* ---- 读档（复刻 for()：遍历 record -> BAIS+DIS 反序列化比对） ---- */
                RecordStore r = RecordStore.openRecordStore(STORE, true);
                int expect = r.getNextRecordID();
                boolean ok = false;
                for (int id = 1; id < expect; id++) {
                    if (r.getRecordSize(id) <= 0) continue;
                    byte[] b = r.getRecord(id);
                    if (b == null) continue;
                    DataInputStream dis = new DataInputStream(new ByteArrayInputStream(b));
                    int magic = dis.readInt();
                    int a = dis.readInt();
                    int c = dis.readInt();
                    boolean flag = dis.readBoolean();
                    String s = dis.readUTF();
                    dis.close();
                    ok = (magic == 0x1010F0F0) && (a == 77) && (c == 88)
                            && flag && s.equals("歪歪歪传战国风云存档测试");
                }
                r.closeRecordStore();
                result = ok ? 2 : 3;
            }
        } catch (Exception e) {
            result = 3;
        }
        Canvas cv = new Canvas() {
            protected void paint(Graphics g) {
                int w = getWidth(), h = getHeight();
                g.setColor(0, 0, 0);
                g.fillRect(0, 0, w, h);
                int col = (result == 1) ? 0x0000FF : (result == 2) ? 0x00FF00 : 0xFF0000;
                g.setColor(col);
                for (int y = 0; y < h; y += 32)
                    for (int x = 0; x < w; x += 32)
                        g.fillRect(x + 8, y + 8, 16, 16);
            }
        };
        Display.getDisplay(this).setCurrent(cv);
        /* 循环重绘（headless/e2e 靠 paint 帧抓屏） */
        final Canvas fcv = cv;
        new Thread(new Runnable() {
            public void run() {
                while (true) {
                    fcv.repaint();
                    try { Thread.sleep(30); } catch (InterruptedException e) { }
                }
            }
        }).start();
    }

    public void pauseApp() { }
    public void destroyApp(boolean u) { }
}
