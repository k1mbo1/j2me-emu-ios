package javax.microedition.media.control;

import javax.microedition.media.Control;

/* VolumeControl（JSR-135 标准包名）：音量控制（0-100） */
public interface VolumeControl extends Control {
    int setLevel(int level);   /* 返回实际生效的音量（钳到 0-100） */
    int getLevel();
    void setMute(boolean mute);
    boolean isMuted();
}
