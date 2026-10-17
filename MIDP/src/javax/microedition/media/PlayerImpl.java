package javax.microedition.media;

import java.util.Vector;

/**
 * PlayerImpl: JSR-135 状态机实现。
 * tone 模式（device://tone）经 ToneControl.setSequence 注入音序后延迟建 native 句柄。
 * END_OF_MEDIA 在 getState() 轮询时惰性检测（游戏线程内同步回调）。
 */
public final class PlayerImpl implements Player, ToneControl {

    private byte[] data;          /* 原始字节（可 null = tone 模式） */
    private byte[] toneSeq;       /* ToneControl 音序 */
    private final String mime;
    private boolean toneMode;
    private int h = -1;           /* native 句柄 */
    private int state = Player.UNREALIZED;
    private int loopCount = 1;
    private Vector listeners;
    private VolCtl volCtl;

    /* ===== natives（Bridge 注册） ===== */
    static native int  nCreate(byte[] d, String type);
    static native int  nCreateToneSeq(byte[] seq);
    static native int  nStart(int h);
    static native int  nStop(int h);
    static native void nClose(int h);
    static native int  nPrefetch(int h);
    static native void nSetLoopCount(int h, int count);
    static native void nSetVolume(int h, int vol);
    static native int  nGetVolume(int h);
    static native int  nState(int h);
    static native long nDuration(int h);   /* ms */
    static native long nPosition(int h);   /* ms */

    PlayerImpl(byte[] d, String type) {
        this.data = d;
        this.mime = type;
        this.toneMode = (d == null);   /* device locator（等 ToneControl.setSequence） */
        volCtl = new VolCtl();
    }

    /* ===== 状态机 ===== */

    private void ensureRealized() throws MediaException {
        if (state == Player.CLOSED) throw new MediaException("player closed");
        if (state == Player.UNREALIZED) realize();
    }

    public void realize() throws MediaException {
        if (state == Player.CLOSED) throw new MediaException("player closed");
        if (state != Player.UNREALIZED) return;
        if (!toneMode && data != null) {
            h = nCreate(data, mime);
            if (h < 0) throw new MediaException("cannot realize " + mime);
        }
        state = Player.REALIZED;
    }

    public void prefetch() throws MediaException {
        if (state == Player.CLOSED) throw new MediaException("player closed");
        if (state == Player.STARTED || state == Player.PREFETCHED) return;
        ensureRealized();
        if (toneMode && h < 0 && toneSeq != null) {
            h = nCreateToneSeq(toneSeq);
            if (h < 0) throw new MediaException("bad tone sequence");
        }
        if (h >= 0) {
            if (nPrefetch(h) != 0) throw new MediaException("prefetch failed");
            if (loopCount != 1) nSetLoopCount(h, loopCount);
            if (volCtl.level != 100 || volCtl.muted) nSetVolume(h, volCtl.muted ? 0 : volCtl.level);
        }
        state = Player.PREFETCHED;
    }

    public void start() throws MediaException {
        if (state == Player.CLOSED) throw new MediaException("player closed");
        if (state == Player.STARTED) return;
        prefetch();
        if (h >= 0 && nStart(h) != 0) throw new MediaException("start failed");
        state = Player.STARTED;
        notify(PlayerListener.STARTED, null);
    }

    public void stop() {
        if (state != Player.STARTED) return;
        if (h >= 0) nStop(h);
        state = Player.PREFETCHED;
        notify(PlayerListener.STOPPED, null);
    }

    public void deallocate() {
        if (state == Player.STARTED) stop();
        if (state == Player.REALIZED || state == Player.PREFETCHED) {
            if (h >= 0) { nClose(h); h = -1; }
            state = Player.UNREALIZED;
        }
    }

    public void close() {
        if (state == Player.CLOSED) return;
        if (h >= 0) { nClose(h); h = -1; }
        state = Player.CLOSED;
        notify(PlayerListener.CLOSED, null);
        listeners = null;
    }

    public int getState() {
        if (state == Player.STARTED && h >= 0 && nState(h) == 0) {
            state = Player.PREFETCHED;
            notify(PlayerListener.END_OF_MEDIA, null);
        }
        return state;
    }

    public long getDuration() {
        if (state != Player.PREFETCHED && state != Player.STARTED) return -1;
        if (h < 0) return -1;
        long ms = nDuration(h);
        return ms < 0 ? -1 : ms * 1000;
    }

    public long getMediaTime() {
        if (h < 0) return -1;
        return nPosition(h) * 1000;
    }

    public long setMediaTime(long now) {
        /* 平台层无 seek 通道：返回当前位置（多数游戏不依赖返回值做精确 seek） */
        return getMediaTime();
    }

    public void setLoopCount(int count) {
        if (state == Player.STARTED) return;
        loopCount = count;
        if (h >= 0) nSetLoopCount(h, count);
    }

    public String getContentType() { return mime; }

    public Control getControl(String type) {
        if (type == null) return null;
        if (type.equalsIgnoreCase("VolumeControl")) return volCtl;
        if (type.equalsIgnoreCase("ToneControl") && toneMode) return this;
        return null;
    }

    public Control[] getControls() {
        if (toneMode) return new Control[] { volCtl, this };
        return new Control[] { volCtl };
    }

    public void addPlayerListener(PlayerListener l) {
        if (l == null) return;
        if (listeners == null) listeners = new Vector();
        if (!listeners.contains(l)) listeners.addElement(l);
    }

    public void removePlayerListener(PlayerListener l) {
        if (listeners != null) listeners.removeElement(l);
    }

    private void notify(String event, Object data) {
        if (listeners == null) return;
        for (int i = 0; i < listeners.size(); i++) {
            PlayerListener l = (PlayerListener) listeners.elementAt(i);
            l.playerUpdate(this, event, data);
        }
    }

    /* ===== ToneControl ===== */

    public void setSequence(byte[] sequence) {
        if (state == Player.STARTED) throw new IllegalStateException("started");
        if (sequence == null) throw new NullPointerException();
        toneSeq = sequence;
        if (h >= 0) { nClose(h); h = -1; }   /* 下次 prefetch 重建 */
        if (state == Player.PREFETCHED || state == Player.REALIZED) state = Player.REALIZED;
    }

    /* ===== VolumeControl 实现 ===== */

    final class VolCtl implements VolumeControl {
        int level = 100;
        boolean muted = false;

        public void setLevel(int l) {
            if (l < 0) l = 0;
            if (l > 100) l = 100;
            level = l;
            if (h >= 0) nSetVolume(h, muted ? 0 : l);
            notify(PlayerListener.VOLUME_CHANGED, null);
        }
        public int getLevel() {
            if (h >= 0) {
                int v = nGetVolume(h);
                if (v >= 0) return v;
            }
            return level;
        }
        public void setMute(boolean m) {
            muted = m;
            if (h >= 0) nSetVolume(h, m ? 0 : level);
        }
        public boolean isMuted() { return muted; }
    }
}
