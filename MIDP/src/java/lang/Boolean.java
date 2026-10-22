package java.lang;

public final class Boolean {
    public static final Boolean TRUE = new Boolean(true);
    public static final Boolean FALSE = new Boolean(false);

    private final boolean value;

    public Boolean(boolean v) { value = v; }
    public Boolean(String s) { value = "true".equalsIgnoreCase(s); }

    public boolean booleanValue() { return value; }

    public static Boolean valueOf(boolean b) { return b ? TRUE : FALSE; }
    public static Boolean valueOf(String s) { return "true".equalsIgnoreCase(s) ? TRUE : FALSE; }
    public static boolean parseBoolean(String s) { return "true".equalsIgnoreCase(s); }

    public String toString() { return value ? "true" : "false"; }
    public boolean equals(Object o) { return (o instanceof Boolean) && ((Boolean) o).value == value; }
    public int hashCode() { return value ? 1231 : 1237; }
}
