package java.lang;

/* Runtime：精简实现（游戏常用于 gc / 内存查询 / exit）。
 * exit 委托 System.exit（Bridge 实现为结束 MIDlet）。 */
public class Runtime {

    private static final Runtime instance = new Runtime();

    private Runtime() { }

    public static Runtime getRuntime() { return instance; }

    public void gc() { }

    public void runFinalization() { }

    public long freeMemory() { return 2L * 1024 * 1024; }

    public long totalMemory() { return 4L * 1024 * 1024; }

    public void exit(int status) { System.exit(status); }
}
