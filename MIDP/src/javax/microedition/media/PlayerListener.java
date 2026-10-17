package javax.microedition.media;

/* PlayerListener: 播放事件回调（游戏线程内同步触发） */
public interface PlayerListener {
    String STARTED       = "started";
    String STOPPED       = "stopped";
    String END_OF_MEDIA  = "endOfMedia";
    String DURATION_UPDATED = "durationUpdated";
    String VOLUME_CHANGED   = "volumeChanged";
    String CLOSED        = "closed";

    void playerUpdate(Player player, String event, Object eventData);
}
