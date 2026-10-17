package javax.microedition.media.control;

import javax.microedition.media.Control;

/* VolumeControl（JSR-135 标准包名）：音量控制（0-100） */
public interface VolumeControl extends Control {
    void setLevel(int level);
    int getLevel();
    void setMute(boolean mute);
    boolean isMuted();
}
