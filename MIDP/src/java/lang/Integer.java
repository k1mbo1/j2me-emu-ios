package java.lang;

public final class Integer extends Number {
    public static final int MIN_VALUE = 0x80000000;
    public static final int MAX_VALUE = 0x7fffffff;

    private final int value;

    public Integer(int v) { value = v; }
    public Integer(String s) throws NumberFormatException { value = parseInt(s); }

    public int intValue() { return value; }
    public long longValue() { return value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }
    public short shortValue() { return (short) value; }
    public byte byteValue() { return (byte) value; }

    public static int parseInt(String s) throws NumberFormatException {
        return parseInt(s, 10);
    }

    public static int parseInt(String s, int radix) throws NumberFormatException {
        if (s == null) throw new NumberFormatException("null");
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX)
            throw new NumberFormatException("radix " + String.valueOf(radix));
        int n = s.length();
        if (n == 0) throw new NumberFormatException("empty");
        int i = 0;
        boolean neg = false;
        char c0 = s.charAt(0);
        if (c0 == '-' || c0 == '+') {
            neg = c0 == '-';
            i = 1;
            if (n == 1) throw new NumberFormatException(s);
        }
        long limit = neg ? 0x80000000L : 0x7fffffffL;
        long r = 0;
        for (; i < n; i++) {
            int d = Character.digit(s.charAt(i), radix);
            if (d < 0) throw new NumberFormatException(s);
            r = r * radix + d;
            if (r > limit) throw new NumberFormatException(s);
        }
        return (int) (neg ? -r : r);
    }

    public static String toString(int v) {
        if (v == MIN_VALUE) return "-2147483648";
        int len = v < 0 ? 2 : 1;
        int t = v < 0 ? -v : v;
        while (t >= 10) { len++; t /= 10; }
        char[] buf = new char[len];
        int p = len;
        t = v < 0 ? -v : v;
        do { p--; buf[p] = (char) ('0' + t % 10); t /= 10; } while (t != 0);
        if (v < 0) buf[0] = '-';
        return new String(buf);
    }

    public static String toString(int v, int radix) {
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX) radix = 10;
        if (radix == 10) return toString(v);
        long t = v;
        boolean neg = t < 0;
        if (neg) t = -t;
        char[] tmp = new char[33];
        int p = tmp.length;
        do {
            p--;
            tmp[p] = Character.forDigit((int) (t % radix), radix);
            t /= radix;
        } while (t != 0);
        if (neg) { p--; tmp[p] = '-'; }
        char[] r = new char[tmp.length - p];
        System.arraycopy(tmp, p, r, 0, r.length);
        return new String(r);
    }

    public static String toHexString(int v) {
        if (v == 0) return "0";
        char[] tmp = new char[8];
        int p = 8;
        int t = v;
        while (t != 0) {
            p--;
            tmp[p] = Character.forDigit(t & 0xF, 16);
            t >>>= 4;
        }
        char[] r = new char[8 - p];
        System.arraycopy(tmp, p, r, 0, r.length);
        return new String(r);
    }

    public static String toBinaryString(int v) {
        if (v == 0) return "0";
        char[] tmp = new char[32];
        int p = 32;
        int t = v;
        while (t != 0) { p--; tmp[p] = (char) ('0' + (t & 1)); t >>>= 1; }
        char[] r = new char[32 - p];
        System.arraycopy(tmp, p, r, 0, r.length);
        return new String(r);
    }

    public static String toOctalString(int v) {
        if (v == 0) return "0";
        char[] tmp = new char[11];
        int p = 11;
        int t = v;
        while (t != 0) { p--; tmp[p] = (char) ('0' + (t & 7)); t >>>= 3; }
        char[] r = new char[11 - p];
        System.arraycopy(tmp, p, r, 0, r.length);
        return new String(r);
    }

    public static Integer valueOf(int v) { return new Integer(v); }
    public static Integer valueOf(String s) throws NumberFormatException { return valueOf(parseInt(s)); }

    public String toString() { return toString(value); }
    public boolean equals(Object o) { return (o instanceof Integer) && ((Integer) o).value == value; }
    public int hashCode() { return value; }
}
