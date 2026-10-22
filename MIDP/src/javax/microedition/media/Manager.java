package javax.microedition.media;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import javax.microedition.io.Connector;
import javax.microedition.io.InputConnection;

/**
 * Manager: MMAPI 入口（JSR-135）。
 * 音频合成/播放走 native（Bridge 软件音源 + 平台播放器）。
 */
public final class Manager {

    public static final String TONE_DEVICE_LOCATOR = "device://tone";
    public static final String MIDI_DEVICE_LOCATOR = "device://midi";

    private Manager() { }

    /* ===== 单音符（多数老游戏用这个做音效）===== */
    public static void playTone(int note, int duration, int volume) throws MediaException {
        if (note < 0 || note > 127 || volume < 0 || volume > 100)
            throw new IllegalArgumentException();
        if (nPlayTone(note, duration, volume) != 0)
            throw new MediaException("tone play failed");
    }
    static native int nPlayTone(int note, int duration, int volume);

    /* ===== 流式创建 ===== */
    public static Player createPlayer(InputStream stream, String type)
            throws MediaException, IOException {
        if (stream == null) throw new IllegalArgumentException("stream");
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = stream.read(buf)) > 0) bos.write(buf, 0, n);
        return new PlayerImpl(bos.toByteArray(), type);
    }

    /* ===== locator 创建（device://tone / http(s):// 音频 URL）===== */
    public static Player createPlayer(String locator) throws MediaException, IOException {
        if (locator == null) throw new NullPointerException();
        if (TONE_DEVICE_LOCATOR.equals(locator))
            return new PlayerImpl(null, "tone");
        if (MIDI_DEVICE_LOCATOR.equals(locator))
            return new PlayerImpl(null, "tone");   /* MIDI device 走 ToneControl 序列 */
        if (locator.startsWith("http://") || locator.startsWith("https://")) {
            InputConnection c = (InputConnection) Connector.open(locator);
            InputStream in = c.openInputStream();
            try {
                ByteArrayOutputStream bos = new ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                int n;
                while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
                String t = guessType(locator);
                return new PlayerImpl(bos.toByteArray(), t);
            } finally {
                try { in.close(); } catch (IOException e) { }
            }
        }
        throw new MediaException("unsupported locator: " + locator);
    }

    private static String guessType(String url) {
        String u = url.toLowerCase();
        if (u.endsWith(".mid") || u.endsWith(".midi")) return "audio/midi";
        if (u.endsWith(".mp3")) return "audio/mpeg";
        if (u.endsWith(".wav")) return "audio/x-wav";
        return null;   /* 交给 native 嗅探魔数 */
    }

    public static String[] getSupportedContentTypes(String protocol) {
        return new String[] { "audio/x-wav", "audio/wav", "audio/mpeg", "audio/midi",
                              "audio/x-mid", "audio/x-tone-seq", "audio/aac" };
    }

    public static String[] getSupportedProtocols(String content_type) {
        return new String[] { "http", "https", "device" };
    }
}
