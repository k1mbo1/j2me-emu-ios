package java.util;

/** 骨架 Hashtable（分离链，CLDC 同款接口，无锁简化） */
public class Hashtable {
    private HashtableEntry[] table;
    private int count;

    public Hashtable() { this(11); }

    public Hashtable(int initCap) {
        if (initCap < 1) initCap = 11;
        table = new HashtableEntry[initCap];
    }

    public int size() { return count; }
    public boolean isEmpty() { return count == 0; }

    public Object get(Object key) {
        if (key == null) throw new NullPointerException();
        int h = key.hashCode() & 0x7fffffff;
        for (HashtableEntry e = table[h % table.length]; e != null; e = e.next) {
            if (e.hash == h && key.equals(e.key)) return e.value;
        }
        return null;
    }

    public Object put(Object key, Object value) {
        if (key == null || value == null) throw new NullPointerException();
        if (count >= table.length * 3 / 4) rehash();
        int h = key.hashCode() & 0x7fffffff;
        int idx = h % table.length;
        for (HashtableEntry e = table[idx]; e != null; e = e.next) {
            if (e.hash == h && key.equals(e.key)) {
                Object old = e.value;
                e.value = value;
                return old;
            }
        }
        table[idx] = new HashtableEntry(h, key, value, table[idx]);
        count++;
        return null;
    }

    public Object remove(Object key) {
        if (key == null) throw new NullPointerException();
        int h = key.hashCode() & 0x7fffffff;
        int idx = h % table.length;
        HashtableEntry prev = null;
        for (HashtableEntry e = table[idx]; e != null; prev = e, e = e.next) {
            if (e.hash == h && key.equals(e.key)) {
                if (prev == null) table[idx] = e.next;
                else prev.next = e.next;
                count--;
                return e.value;
            }
        }
        return null;
    }

    public void clear() {
        for (int i = 0; i < table.length; i++) table[i] = null;
        count = 0;
    }

    public boolean containsKey(Object key) {
        if (key == null) throw new NullPointerException();
        int h = key.hashCode() & 0x7fffffff;
        for (HashtableEntry e = table[h % table.length]; e != null; e = e.next) {
            if (e.hash == h && key.equals(e.key)) return true;
        }
        return false;
    }

    public boolean contains(Object value) {
        if (value == null) throw new NullPointerException();
        for (int i = 0; i < table.length; i++) {
            for (HashtableEntry e = table[i]; e != null; e = e.next) {
                if (value.equals(e.value)) return true;
            }
        }
        return false;
    }

    public Enumeration keys() {
        Object[] a = new Object[count];
        int n = 0;
        for (int i = 0; i < table.length; i++)
            for (HashtableEntry e = table[i]; e != null; e = e.next)
                a[n++] = e.key;
        return new HashtableEnumerator(a);
    }

    public Enumeration elements() {
        Object[] a = new Object[count];
        int n = 0;
        for (int i = 0; i < table.length; i++)
            for (HashtableEntry e = table[i]; e != null; e = e.next)
                a[n++] = e.value;
        return new HashtableEnumerator(a);
    }

    private void rehash() {
        HashtableEntry[] nt = new HashtableEntry[table.length * 2 + 1];
        for (int i = 0; i < table.length; i++) {
            HashtableEntry e = table[i];
            while (e != null) {
                HashtableEntry nx = e.next;
                int idx = e.hash % nt.length;
                e.next = nt[idx];
                nt[idx] = e;
                e = nx;
            }
        }
        table = nt;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();
        sb.append('{');
        boolean first = true;
        for (int i = 0; i < table.length; i++) {
            for (HashtableEntry e = table[i]; e != null; e = e.next) {
                if (!first) sb.append(", ");
                first = false;
                sb.append(e.key == null ? "null" : e.key.toString());
                sb.append('=');
                sb.append(e.value == null ? "null" : e.value.toString());
            }
        }
        sb.append('}');
        return sb.toString();
    }
}
