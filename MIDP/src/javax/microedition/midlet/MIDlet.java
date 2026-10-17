package javax.microedition.midlet;

/**
 * MIDlet 生命周期骨架。
 * Bridge C 层在启动时顺序调用：无参构造器 -> startApp()
 * notifyDestroyed -> native 通知 Bridge 退出应用
 */
public abstract class MIDlet {

    protected MIDlet() { }

    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;

    /** 通知系统 MIDlet 已完成，Bridge 收到后退出 */
    public final void notifyDestroyed() {
        nativeMidletNotify(2);
    }

    /** 通知系统 MIDlet 自愿暂停 */
    public final void notifyPaused() {
        nativeMidletNotify(1);
    }

    /** 请求恢复运行 */
    public final void resumeRequest() {
        nativeMidletNotify(0);
    }

    /** 读取 JAD/JAR manifest 属性（Bridge 提供） */
    public final String getAppProperty(String key) {
        return nativeGetAppProperty(key);
    }

    static native void nativeMidletNotify(int state);
    static native String nativeGetAppProperty(String key);
}
