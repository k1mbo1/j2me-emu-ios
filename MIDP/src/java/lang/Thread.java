package java.lang;

/** 骨架 Thread：currentThread/sleep/start/interrupt/isInterrupted/isAlive 为 native */
public class Thread implements Runnable {
    Runnable target;
    String name;

    public Thread() { }
    public Thread(Runnable r) { target = r; }
    public Thread(String n) { name = n; }
    public Thread(Runnable r, String n) { target = r; name = n; }

    public static native Thread currentThread();
    public static native void sleep(long ms) throws InterruptedException;
    public native void start();
    public native void interrupt();
    public native boolean isInterrupted();
    public native boolean isAlive();

    public static boolean interrupted() { return currentThread().isInterrupted(); }

    public void run() { if (target != null) target.run(); }

    public final void setName(String n) { name = n; }
    public final String getName() { return name == null ? "Thread" : name; }
    public final void setPriority(int p) { }
    public final int getPriority() { return 5; }
    public static int activeCount() { return 1; }
    public static void yield() { }

    public final void join() throws InterruptedException {
        while (isAlive()) sleep(10);
    }
    public final void join(long ms) throws InterruptedException {
        long end = 0;
        if (ms > 0) end = System.currentTimeMillis() + ms;
        while (isAlive()) {
            if (ms > 0 && System.currentTimeMillis() >= end) break;
            sleep(10);
        }
    }
}
