package com.nokia.mid.sound;

import java.io.ByteArrayInputStream;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;

/* Sound（Nokia UI API）：老式音效接口。
 * 兼容实现：WAV 数据走 MMAPI 播放，单音频率走 Manager.playTone。 */
public class Sound {

    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV  = 5;

    public static final int SOUND_PLAYING       = 0;
    public static final int SOUND_STOPPED       = 1;
    public static final int SOUND_UNINITIALIZED = 3;

    private byte[] data;          /* 音频数据（可 null = 单音模式） */
    private int type;
    private int freq;             /* 单音频率 */
    private long durMs;           /* 单音时长 */
    private int gain = 255;       /* 0-255 */
    private SoundListener listener;
    private Player wavPlayer;     /* WAV 播放句柄 */

    public Sound(byte[] data, int type) {
        this.data = data;
        this.type = type;
    }

    public Sound(int frequency, long durationMs) {
        this.freq = frequency;
        this.durMs = durationMs;
    }

    public void init(byte[] data, int type) {
        stop();
        this.data = data;
        this.type = type;
    }

    public void init(int frequency, long durationMs) {
        stop();
        this.data = null;
        this.freq = frequency;
        this.durMs = durationMs;
    }

    public void play() {
        playWav(1);
    }

    /** Nokia 扩展：loopCount 次循环播放（-1 = 无限循环；0 视作 1） */
    public void play(int loopCount) {
        playWav(loopCount <= 0 ? (loopCount == 0 ? 1 : loopCount) : loopCount);
    }

    private void playWav(int loopCount) {
        try {
            if (data != null && type == FORMAT_WAV) {
                if (wavPlayer != null) wavPlayer.close();
                wavPlayer = Manager.createPlayer(new ByteArrayInputStream(data), "audio/x-wav");
                if (loopCount != 1) wavPlayer.setLoopCount(loopCount);
                wavPlayer.start();
            } else if (data == null && freq > 0) {
                Manager.playTone(freq, (int) durMs, gain * 100 / 255);
            }
        } catch (Exception e) { /* 音效失败不阻断游戏 */ }
        if (listener != null) listener.sound(this, SOUND_PLAYING);
    }

    public void stop() {
        if (wavPlayer != null) {
            wavPlayer.close();
            wavPlayer = null;
        }
        if (listener != null) listener.sound(this, SOUND_STOPPED);
    }

    public void release() {
        stop();
        data = null;
        wavPlayer = null;
    }

    public int getState() {
        return SOUND_STOPPED;
    }

    public void setGain(int g) {
        if (g < 0) g = 0;
        if (g > 255) g = 255;
        gain = g;
    }

    public int getGain() { return gain; }

    public void setSoundListener(SoundListener l) { listener = l; }
}
