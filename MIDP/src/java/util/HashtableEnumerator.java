package java.util;

class HashtableEnumerator implements Enumeration {
    private Object[] items;
    private int i = 0;

    HashtableEnumerator(Object[] items) { this.items = items; }

    public boolean hasMoreElements() { return i < items.length; }

    public Object nextElement() {
        if (i >= items.length) throw new NoSuchElementException();
        return items[i++];
    }
}
