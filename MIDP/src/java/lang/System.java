package java.lang;

public final class System {
    public static final java.io.PrintStream out = new java.io.PrintStream();
    public static final java.io.PrintStream err = new java.io.PrintStream();

    private System() { }

    public static native void arraycopy(Object src, int srcPos, Object dst, int dstPos, int length);
    public static native long currentTimeMillis();
    public static native long nanoTime();
    public static native String getProperty(String key);
    public static native void exit(int code);

    public static void gc() { }
    public static int identityHashCode(Object o) { return o == null ? 0 : o.hashCode(); }
}
