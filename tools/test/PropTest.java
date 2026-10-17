import javax.microedition.midlet.MIDlet;

/**
 * PropTest：验证 getAppProperty（读 jar 的 MANIFEST.MF 属性，iOS 主类发现依赖此链路）。
 */
public class PropTest extends MIDlet {

    public static void main(String[] args) {
        PropTest p = new PropTest();
        String m1 = p.getAppProperty("MIDlet-1");
        String name = p.getAppProperty("MIDlet-Name");
        String prof = p.getAppProperty("MicroEdition-Profile");
        String none = p.getAppProperty("Not-Exist-Key");
        if (m1 == null || !m1.trim().equals("Snake,,SnakeGame"))
            throw new RuntimeException("PT: MIDlet-1 wrong: [" + m1 + "]");
        if (name == null || !name.trim().equals("Snake"))
            throw new RuntimeException("PT: MIDlet-Name wrong: [" + name + "]");
        if (prof == null || !prof.trim().equals("MIDP-2.0"))
            throw new RuntimeException("PT: Profile wrong: [" + prof + "]");
        if (none != null)
            throw new RuntimeException("PT: Not-Exist-Key should be null");
        System.out.println("PT: OK MIDlet-1=[" + m1 + "] name=[" + name
                + "] profile=[" + prof + "]");
    }

    protected void startApp() { }
    protected void pauseApp() { }
    protected void destroyApp(boolean unconditional) { }
}
