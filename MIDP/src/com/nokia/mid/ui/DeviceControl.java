package com.nokia.mid.ui;

/* DeviceControl（Nokia UI API）：背光/振动控制。兼容空实现。 */
public final class DeviceControl {

    private DeviceControl() { }

    public static void setLights(int num, int level) { }
    public static void flashLights(long duration) { }

    public static boolean startVibra(int duration) {
        return false;
    }
    public static void stopVibra() { }
}
