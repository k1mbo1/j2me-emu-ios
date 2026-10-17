import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import javax.microedition.media.Control;
import javax.microedition.media.Manager;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.ToneControl;
import javax.microedition.media.control.VolumeControl;

/* MediaTest - MMAPI 链路验证（headless 媒体平台落盘 WAV）
 * 覆盖: playTone / WAV Player 状态机 / VolumeControl / ToneControl 音序 /
 *       MIDI 文件合成 / PlayerListener 事件 / getResourceAsStream
 */
public class MediaTest implements PlayerListener {
    static int pass = 0, fail = 0;
    static String lastEvent = null;
    static int eomCount = 0;

    public void playerUpdate(Player p, String event, Object d) {
        lastEvent = event;
        if (PlayerListener.END_OF_MEDIA.equals(event)) eomCount++;
    }

    static void ck(String name, boolean cond) {
        if (cond) { pass++; System.out.println("PASS " + name); }
        else { fail++; System.out.println("FAIL " + name); }
    }

    public static void main(String[] args) throws Exception {
        /* 1. playTone（C4=60, 300ms, 音量 80） */
        try {
            Manager.playTone(60, 300, 80);
            ck("playTone ok", true);
        } catch (MediaException e) {
            ck("playTone ok", false);
        }
        try { Manager.playTone(200, 300, 80); ck("playTone range check", false); }
        catch (IllegalArgumentException e) { ck("playTone range check", true); }

        /* 2. 最小 WAV（44 头 + 100 样本）：手工构造 */
        byte[] wav = new byte[44 + 200];
        setLE32(wav, 0, 0x46464952);            /* RIFF */
        setLE32(wav, 4, 36 + 200);
        setLE32(wav, 8, 0x45564157);            /* WAVE */
        setLE32(wav, 12, 0x20746D66);           /* "fmt " */
        setLE32(wav, 16, 16);
        setLE16(wav, 20, 1);
        setLE16(wav, 22, 1);
        setLE32(wav, 24, 22050);
        setLE32(wav, 28, 44100);
        setLE16(wav, 32, 2);
        setLE16(wav, 34, 16);
        setLE32(wav, 36, 0x61746164);           /* data */
        setLE32(wav, 40, 200);
        for (int i = 0; i < 100; i++) setLE16(wav, 44 + i * 2, (i % 50) * 300 - 7500);

        Player p = Manager.createPlayer(new java.io.ByteArrayInputStream(wav), "audio/x-wav");
        MediaTest mt = new MediaTest();
        p.addPlayerListener(mt);
        ck("initial state UNREALIZED", p.getState() == Player.UNREALIZED);
        p.realize();
        ck("realize -> REALIZED", p.getState() == Player.REALIZED);
        p.prefetch();
        ck("prefetch -> PREFETCHED", p.getState() == Player.PREFETCHED);
        ck("duration == 4000us", p.getDuration() == 4000);   /* 100 样本 @22050 -> 4ms */
        p.start();
        ck("start -> STARTED", p.getState() == Player.STARTED);
        ck("STARTED event fired", PlayerListener.STARTED.equals(lastEvent));
        p.stop();
        ck("stop -> PREFETCHED", p.getState() == Player.PREFETCHED);
        ck("STOPPED event fired", PlayerListener.STOPPED.equals(lastEvent));
        p.start();
        ck("restart ok", p.getState() == Player.STARTED);
        p.close();
        ck("close -> CLOSED", p.getState() == Player.CLOSED);
        try { p.start(); ck("start after close throws", false); }
        catch (MediaException e) { ck("start after close throws", true); }

        /* 3. VolumeControl */
        Player pv = Manager.createPlayer(new java.io.ByteArrayInputStream(wav), "audio/x-wav");
        VolumeControl vc = (VolumeControl) pv.getControl("VolumeControl");
        ck("getControl VolumeControl", vc != null);
        pv.realize();
        pv.prefetch();
        vc.setLevel(55);
        ck("setLevel/getLevel", vc.getLevel() == 55);
        vc.setMute(true);
        ck("isMuted", vc.isMuted());
        vc.setMute(false);
        ck("unmute", !vc.isMuted());
        pv.close();

        /* 4. ToneControl 音序 */
        Player pt = Manager.createPlayer(Manager.TONE_DEVICE_LOCATOR);
        ToneControl tc = (ToneControl) pt.getControl("ToneControl");
        ck("getControl ToneControl", tc != null);
        byte[] seq = {
            ToneControl.VERSION, 120, ToneControl.VERSION, 64,
            60, 32,   /* C4 半拍 */
            64, 32,   /* E4 */
            67, 32,   /* G4 */
            ToneControl.SILENCE, 32,
            72, 64    /* C5 一拍 */
        };
        tc.setSequence(seq);
        pt.realize();
        pt.prefetch();
        ck("tone seq duration > 0", pt.getDuration() > 0);
        pt.start();
        ck("tone seq started", pt.getState() == Player.STARTED);
        pt.close();

        /* 5. MIDI 文件合成（手工构造最小 SMF format 0） */
        byte[] mid = {
            0x4D,0x54,0x68,0x64, 0,0,0,6, 0,0, 0,1, 0,120,          /* MThd fmt0 1trk div120 */
            0x4D,0x54,0x72,0x6B, 0,0,0,30,                          /* MTrk len=30 */
            0, (byte)0xFF,0x51,0x03, 0x07,(byte)0xA1,0x20,          /* tempo 500000 */
            0, (byte)0xC0,0x00,                                      /* program 0 */
            0, (byte)0x90,60,0x40,                                   /* note on C4 */
            60, (byte)0x80,60,0x40,                                  /* 120tick 后 off */
            0, (byte)0xFF,0x2F,0x00                                  /* EOT */
        };
        Player pm = Manager.createPlayer(new java.io.ByteArrayInputStream(mid), "audio/midi");
        pm.realize();
        pm.prefetch();
        long d = pm.getDuration();
        ck("midi duration > 400ms", d > 400000);   /* 120tick@div120,120bpm = 500ms */
        pm.close();

        /* 6. getResourceAsStream（绝对路径 + 相对包路径） */
        InputStream in = MediaTest.class.getResourceAsStream("/MediaTest.class");
        ck("getResourceAsStream abs", in != null && in.read() == 0xCA);
        InputStream in2 = MediaTest.class.getResourceAsStream("MediaTest.class");
        ck("getResourceAsStream rel", in2 != null && in2.read() == 0xCA);
        InputStream in3 = MediaTest.class.getResourceAsStream("/no_such.thing");
        ck("getResourceAsStream miss", in3 == null);

        System.out.println("pass=" + pass + " fail=" + fail);
        if (fail > 0) System.exit(1);
        System.out.println("MEDIA TEST PASS");
    }

    static void setLE32(byte[] b, int off, int v) {
        b[off] = (byte)v; b[off+1] = (byte)(v >> 8);
        b[off+2] = (byte)(v >> 16); b[off+3] = (byte)(v >> 24);
    }
    static void setLE16(byte[] b, int off, int v) {
        b[off] = (byte)v; b[off+1] = (byte)(v >> 8);
    }
}
