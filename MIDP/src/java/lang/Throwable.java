package java.lang;

/** 骨架 Throwable：字段 message 被 C 层依赖，勿改名 */
public class Throwable {
    String message;

    public Throwable() { }
    public Throwable(String m) { message = m; }

    public String getMessage() { return message; }
    public String getLocalizedMessage() { return message; }

    public String toString() {
        String cn = getClass().getName();
        return message == null ? cn : cn + ": " + message;
    }

    public void printStackTrace() { System.err.println(toString()); }

    public Throwable fillInStackTrace() { return this; }
}
