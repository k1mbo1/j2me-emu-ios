package java.lang;

public final class Long extends Number {
    public static final long MIN_VALUE = 0x8000000000000000L;
    public static final long MAX_VALUE = 0x7fffffffffffffffL;

    private final long value;

    public Long(long v) { value = v; }
    public Long(String s) throws NumberFormatException { value = parseLong(s); }

    public int intValue() { return (int) value; }
    public long longValue() { return value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }

    public static long parseLong(String s) throws NumberFormatException {
        return parseLong(s, 10);
    }

    public static long parseLong(String s, int radix) throws NumberFormatException {
        if (s == null) throw new NumberFormatException("null");
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX)
            throw new NumberFormatException("radix");
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
        /* 负数累积避免溢出（OpenJDK 同款算法） */
        long limit = neg ? Long.MIN_VALUE : -Long.MAX_VALUE;
        long multmin = limit / radix;
        long r = 0;
        for (; i < n; i++) {
            int d = Character.digit(s.charAt(i), radix);
            if (d < 0) throw new NumberFormatException(s);
            if (r < multmin) throw new NumberFormatException(s);
            r *= radix;
            if (r < limit + d) throw new NumberFormatException(s);
            r -= d;
        }
        return neg ? r : -r;
    }

    public static String toString(long v) {
        if (v == MIN_VALUE) return "-9223372036854775808";
        int len = v < 0 ? 2 : 1;
        long t = v < 0 ? -v : v;
        while (t >= 10) { len++; t /= 10; }
        char[] buf = new char[len];
        int p = len;
        t = v < 0 ? -v : v;
        do { p--; buf[p] = (char) ('0' + (int) (t % 10)); t /= 10; } while (t != 0);
        if (v < 0) buf[0] = '-';
        return new String(buf);
    }

    public static String toString(long v, int radix) {
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX) radix = 10;
        if (radix == 10) return toString(v);
        long t = v;
        boolean neg = t < 0;
        if (neg) t = -t;
        char[] tmp = new char[65];
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

    public static String toHexString(long v) {
        if (v == 0) return "0";
        char[] tmp = new char[16];
        int p = 16;
        while (v != 0) {
            p--;
            tmp[p] = Character.forDigit((int) (v & 0xFL), 16);
            v >>>= 4;
        }
        char[] r = new char[16 - p];
        System.arraycopy(tmp, p, r, 0, r.length);
        return new String(r);
    }

    public static Long valueOf(long v) { return new Long(v); }
    public static Long valueOf(String s) throws NumberFormatException { return valueOf(parseLong(s)); }

    public String toString() { return toString(value); }
    public boolean equals(Object o) { return (o instanceof Long) && ((Long) o).value == value; }
    public int hashCode() { return (int) (value ^ (value >>> 32)); }
}
