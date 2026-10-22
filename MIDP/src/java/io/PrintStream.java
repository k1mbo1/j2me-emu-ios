package java.io;

/** 骨架 PrintStream：print(String) 为 native（vm.c 输出到日志） */
public class PrintStream {
    public PrintStream() { }

    public native void print(String s);

    public void print(char c) { print(String.valueOf(c)); }
    public void print(char[] c) { print(new String(c)); }
    public void print(boolean b) { print(b ? "true" : "false"); }
    public void print(int i) { print(Integer.toString(i)); }
    public void print(long l) { print(Long.toString(l)); }
    public void print(float f) { print(Float.toString(f)); }
    public void print(double d) { print(Double.toString(d)); }
    public void print(Object o) { print(o == null ? "null" : o.toString()); }

    public void println() { print("\n"); }
    public void println(String s) { print(s); print("\n"); }
    public void println(char c) { print(c); print("\n"); }
    public void println(char[] c) { print(c); print("\n"); }
    public void println(boolean b) { print(b); print("\n"); }
    public void println(int i) { print(i); print("\n"); }
    public void println(long l) { print(l); print("\n"); }
    public void println(float f) { print(f); print("\n"); }
    public void println(double d) { print(d); print("\n"); }
    public void println(Object o) { print(o); print("\n"); }

    public void flush() { }
}
