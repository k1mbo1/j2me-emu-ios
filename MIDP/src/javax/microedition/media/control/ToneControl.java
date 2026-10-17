package javax.microedition.media.control;

import javax.microedition.media.Control;

/* ToneControl（JSR-135 标准包名）：单音音序控制（配 device://tone Player） */
public interface ToneControl extends Control {
    byte VERSION     = -2;
    byte BLOCK_START = -3;
    byte BLOCK_END   = -4;
    byte PLAY_BLOCK  = -5;
    byte SET_VOLUME  = -6;
    byte REPEAT      = -7;
    byte SILENCE     = -95;

    void setSequence(byte[] sequence);
}
