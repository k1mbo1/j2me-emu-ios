package javax.microedition.media;

/* Controllable（JSR-135）：可携带 Control 的对象。
 * Player 继承本接口；部分游戏直接引用本类型。 */
public interface Controllable {
    Control getControl(String controlType);
    Control[] getControls();
}
