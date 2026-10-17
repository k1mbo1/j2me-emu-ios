package java.lang;

public final class Character {
    public static final char MIN_VALUE = '\u0000';
    public static final char MAX_VALUE = '\uffff';
    public static final int MIN_RADIX = 2;
    public static final int MAX_RADIX = 36;

    private final char value;

    public Character(char v) { value = v; }

    public char charValue() { return value; }

    public static boolean isDigit(char c) { return c >= '0' && c <= '9'; }
    public static boolean isLetter(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
    public static boolean isLetterOrDigit(char c) { return isLetter(c) || isDigit(c); }
    public static boolean isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
    }
    public static boolean isUpperCase(char c) { return c >= 'A' && c <= 'Z'; }
    public static boolean isLowerCase(char c) { return c >= 'a' && c <= 'z'; }

    public static char toUpperCase(char c) {
        return (c >= 'a' && c <= 'z') ? (char) (c - ('a' - 'A')) : c;
    }
    public static char toLowerCase(char c) {
        return (c >= 'A' && c <= 'Z') ? (char) (c + ('a' - 'A')) : c;
    }

    public static int digit(char c, int radix) {
        int d = -1;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        return (d >= 0 && d < radix) ? d : -1;
    }

    public static char forDigit(int d, int radix) {
        if (d < 0 || d >= radix) return '\0';
        return (char) (d < 10 ? '0' + d : 'a' + d - 10);
    }

    public static Character valueOf(char c) { return new Character(c); }

    public String toString() {
        char[] a = new char[1];
        a[0] = value;
        return new String(a);
    }
    public boolean equals(Object o) { return (o instanceof Character) && ((Character) o).value == value; }
    public int hashCode() { return value; }
}
