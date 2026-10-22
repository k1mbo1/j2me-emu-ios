package java.util;

class HashtableEntry {
    int hash;
    Object key;
    Object value;
    HashtableEntry next;

    HashtableEntry(int h, Object k, Object v, HashtableEntry n) {
        hash = h;
        key = k;
        value = v;
        next = n;
    }
}
