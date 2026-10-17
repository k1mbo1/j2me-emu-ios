import javax.microedition.lcdui.Image;

/** PngTest：PNG 解码验证（main 模式）。t.png: 左半 255,0,0 不透明 / 右半 0,255,0 alpha=128 */
public class PngTest {
    public static void main(String[] args) throws Exception {
        Image img = Image.createImage("/t.png");
        System.out.println("PT: size " + img.getWidth() + "x" + img.getHeight());
        int[] px = new int[64];
        img.getRGB(px, 0, 8, 0, 0, 8, 8);
        System.out.println("PT: (1,1)=0x" + Integer.toHexString(px[9]));
        System.out.println("PT: (6,1)=0x" + Integer.toHexString(px[14]));
        boolean ok = img.getWidth() == 8 && img.getHeight() == 8
                && px[9] == 0xFFFF0000 && px[14] == 0x8000FF00;
        System.out.println("PT: " + (ok ? "PASS" : "FAIL"));
        if (!ok) System.exit(1);
    }
}
