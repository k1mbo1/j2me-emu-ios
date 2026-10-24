package javax.microedition.media;

/* Player: MMAPI 播放器（JSR-135 状态机），继承 Controllable */
public interface Player extends Controllable {
    int UNREALIZED  = 100;
    int REALIZED    = 200;
    int PREFETCHED  = 300;
    int STARTED     = 400;
    int CLOSED      = 0;

    void realize() throws MediaException;
    void prefetch() throws MediaException;
    void start() throws MediaException;
    void stop();
    void deallocate();
    void close();

    int getState();
    long getDuration();           /* TIME_UNKNOWN(-1) 或微秒 */
    long getMediaTime();          /* 微秒 */
    long setMediaTime(long now);  /* 返回实际设置时间（微秒） */
    void setLoopCount(int count); /* STARTED 前设置；-1 无限 */

    String getContentType();
    Control getControl(String type);
    Control[] getControls();

    void addPlayerListener(PlayerListener listener);
    void removePlayerListener(PlayerListener listener);
}
