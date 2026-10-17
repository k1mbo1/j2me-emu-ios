package javax.microedition.media;

/* VolumeControl: 音量控制（0-100） */
public interface VolumeControl extends Control {
    void setLevel(int level);
    int getLevel();
    void setMute(boolean mute);
    boolean isMuted();
}
