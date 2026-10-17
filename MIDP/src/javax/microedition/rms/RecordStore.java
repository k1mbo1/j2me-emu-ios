package javax.microedition.rms;

import java.util.Vector;

/**
 * RecordStore：每个 MIDlet 名字空间独立（Bridge native 层提供按 midletId 隔离的文件存储）。
 * 数据经 native rmsGet/rmsPut/rmsDelete/rmsList 持久化。
 */
public class RecordStore {

    public static final int AUTHMODE_PRIVATE = 0, AUTHMODE_ANY = 1;

    private final String name;
    private int nextId = 1;
    private final Vector listeners = new Vector();
    private boolean open;

    /* 记录表：id -> byte[]（内存缓存，写穿到 native 存储） */
    private final Vector ids = new Vector();
    private final Vector recs = new Vector();
    private static final Vector openStores = new Vector();

    private RecordStore(String name) { this.name = name; }

    public static String[] listRecordStores() {
        return nativeListStores();
    }

    public static RecordStore openRecordStore(String recordStoreName,
                                              boolean createIfNecessary)
            throws RecordStoreException, RecordStoreNotFoundException {
        if (recordStoreName == null || recordStoreName.length() > 32)
            throw new IllegalArgumentException();
        /* 已打开则复用 */
        for (int i = 0; i < openStores.size(); i++) {
            RecordStore rs = (RecordStore) openStores.elementAt(i);
            if (rs.name.equals(recordStoreName)) return rs;
        }
        RecordStore rs = new RecordStore(recordStoreName);
        byte[] data = nativeLoadStore(recordStoreName);
        if (data == null) {
            if (!createIfNecessary)
                throw new RecordStoreNotFoundException(recordStoreName);
            rs.nextId = 1;
        } else {
            rs.deserialize(data);
        }
        rs.open = true;
        openStores.addElement(rs);
        return rs;
    }

    public static RecordStore openRecordStore(String recordStoreName,
                                              boolean createIfNecessary,
                                              int authmode, boolean writable)
            throws RecordStoreException, RecordStoreNotFoundException {
        return openRecordStore(recordStoreName, createIfNecessary);
    }

    public static RecordStore openRecordStore(String recordStoreName,
                                              String vendorName, String suiteName)
            throws RecordStoreException, RecordStoreNotFoundException {
        return openRecordStore(recordStoreName, false);
    }

    public void closeRecordStore() throws RecordStoreException {
        openStores.removeElement(this);
        open = false;
    }

    public String getName() { return name; }

    public int addRecord(byte[] data, int offset, int numBytes)
            throws RecordStoreException, RecordStoreFullException {
        if (data == null && numBytes > 0) throw new NullPointerException();
        if (offset < 0 || numBytes < 0 || (data != null && offset + numBytes > data.length))
            throw new ArrayIndexOutOfBoundsException();
        byte[] rec = new byte[numBytes];
        if (numBytes > 0) System.arraycopy(data, offset, rec, 0, numBytes);
        int id = nextId++;
        ids.addElement(new Integer(id));
        recs.addElement(rec);
        persist();
        for (int i = 0; i < listeners.size(); i++)
            ((RecordListener) listeners.elementAt(i)).recordAdded(this, id);
        return id;
    }

    public void setRecord(int recordId, byte[] newData, int offset, int numBytes)
            throws RecordStoreException, InvalidRecordIDException {
        int idx = indexOf(recordId);
        if (idx < 0) throw new InvalidRecordIDException();
        if (newData == null && numBytes > 0) throw new NullPointerException();
        byte[] rec = new byte[numBytes];
        if (numBytes > 0) System.arraycopy(newData, offset, rec, 0, numBytes);
        recs.setElementAt(rec, idx);
        persist();
        for (int i = 0; i < listeners.size(); i++)
            ((RecordListener) listeners.elementAt(i)).recordChanged(this, recordId);
    }

    public void deleteRecord(int recordId) throws RecordStoreException, InvalidRecordIDException {
        int idx = indexOf(recordId);
        if (idx < 0) throw new InvalidRecordIDException();
        ids.removeElementAt(idx);
        recs.removeElementAt(idx);
        persist();
        for (int i = 0; i < listeners.size(); i++)
            ((RecordListener) listeners.elementAt(i)).recordDeleted(this, recordId);
    }

    public byte[] getRecord(int recordId)
            throws RecordStoreException, InvalidRecordIDException {
        int idx = indexOf(recordId);
        if (idx < 0) throw new InvalidRecordIDException();
        byte[] r = (byte[]) recs.elementAt(idx);
        byte[] out = new byte[r.length];
        System.arraycopy(r, 0, out, 0, r.length);
        return out;
    }

    public int getRecord(int recordId, byte[] buffer, int offset)
            throws RecordStoreException, InvalidRecordIDException {
        int idx = indexOf(recordId);
        if (idx < 0) throw new InvalidRecordIDException();
        byte[] r = (byte[]) recs.elementAt(idx);
        if (offset < 0 || offset + r.length > buffer.length)
            throw new ArrayIndexOutOfBoundsException();
        System.arraycopy(r, 0, buffer, offset, r.length);
        return r.length;
    }

    public int getRecordSize(int recordId)
            throws RecordStoreException, InvalidRecordIDException {
        int idx = indexOf(recordId);
        if (idx < 0) throw new InvalidRecordIDException();
        return ((byte[]) recs.elementAt(idx)).length;
    }

    public int getNextRecordID() throws RecordStoreException { return nextId; }
    public int getNumRecords() throws RecordStoreException { return ids.size(); }

    public long getLastModified() throws RecordStoreException { return 0; }
    public int getVersion() throws RecordStoreException { return 1; }
    public int getSize() throws RecordStoreException { return 1024; }
    public int getSizeAvailable() throws RecordStoreException { return 1024 * 1024; }

    public void setMode(int authmode, boolean writable) { }

    public void addRecordListener(RecordListener l) {
        if (l != null && !listeners.contains(l)) listeners.addElement(l);
    }

    public void removeRecordListener(RecordListener l) {
        listeners.removeElement(l);
    }

    public RecordEnumeration enumerateRecords(RecordFilter filter,
                                              RecordComparator comparator,
                                              boolean keepUpdated) {
        return new EnumImpl(this, filter, comparator, keepUpdated);
    }

    private int indexOf(int id) {
        for (int i = 0; i < ids.size(); i++)
            if (((Integer) ids.elementAt(i)).intValue() == id) return i;
        return -1;
    }

    /* ---- 持久化（native 文件存储） ---- */
    private void persist() {
        nativeSaveStore(name, serialize());
    }

    private byte[] serialize() {
        /* 格式: nextId(u4) n(u4) { id(u4) len(u4) bytes }* */
        int total = 8;
        for (int i = 0; i < recs.size(); i++)
            total += 8 + ((byte[]) recs.elementAt(i)).length;
        byte[] out = new byte[total];
        put4(out, 0, nextId);
        put4(out, 4, ids.size());
        int p = 8;
        for (int i = 0; i < ids.size(); i++) {
            int id = ((Integer) ids.elementAt(i)).intValue();
            byte[] r = (byte[]) recs.elementAt(i);
            put4(out, p, id); p += 4;
            put4(out, p, r.length); p += 4;
            System.arraycopy(r, 0, out, p, r.length); p += r.length;
        }
        return out;
    }

    private void deserialize(byte[] d) {
        if (d == null || d.length < 8) return;
        nextId = get4(d, 0);
        int n = get4(d, 4);
        int p = 8;
        for (int i = 0; i < n && p + 8 <= d.length; i++) {
            int id = get4(d, p); p += 4;
            int len = get4(d, p); p += 4;
            if (p + len > d.length) break;
            byte[] r = new byte[len];
            System.arraycopy(d, p, r, 0, len); p += len;
            ids.addElement(new Integer(id));
            recs.addElement(r);
        }
    }

    private static void put4(byte[] d, int off, int v) {
        d[off] = (byte) (v >> 24);
        d[off + 1] = (byte) (v >> 16);
        d[off + 2] = (byte) (v >> 8);
        d[off + 3] = (byte) v;
    }

    private static int get4(byte[] d, int off) {
        return ((d[off] & 0xFF) << 24) | ((d[off + 1] & 0xFF) << 16)
                | ((d[off + 2] & 0xFF) << 8) | (d[off + 3] & 0xFF);
    }

    /* ---- 枚举实现 ---- */
    private static class EnumImpl implements RecordEnumeration {
        final RecordStore rs;
        final RecordFilter filter;
        final RecordComparator comp;
        final boolean keepUpdated;
        int[] matched;
        int pos;

        EnumImpl(RecordStore rs, RecordFilter f, RecordComparator c, boolean k) {
            this.rs = rs;
            this.filter = f;
            this.comp = c;
            this.keepUpdated = k;
            rebuild();
        }

        public void rebuild() {
            Vector m = new Vector();
            for (int i = 0; i < rs.ids.size(); i++) {
                byte[] r = (byte[]) rs.recs.elementAt(i);
                if (filter == null || filter.matches(r))
                    m.addElement(rs.ids.elementAt(i));
            }
            int n = m.size();
            Integer[] arr = new Integer[n];
            for (int i = 0; i < n; i++) arr[i] = (Integer) m.elementAt(i);
            if (comp != null && n > 1) {
                /* 插入排序（记录少，够用） */
                for (int i = 1; i < n; i++) {
                    Integer key = arr[i];
                    byte[] kr = rs.getRecById(key.intValue());
                    int j = i - 1;
                    while (j >= 0 && comp.compare(rs.getRecById(arr[j].intValue()), kr) == RecordComparator.FOLLOWS) {
                        arr[j + 1] = arr[j];
                        j--;
                    }
                    arr[j + 1] = key;
                }
            }
            matched = new int[n];
            for (int i = 0; i < n; i++) matched[i] = arr[i].intValue();
            pos = 0;
        }

        public boolean hasNextElement() { return pos < matched.length; }
        public boolean hasPreviousElement() { return pos > 0; }

        public byte[] nextRecord() throws InvalidRecordIDException, RecordStoreException {
            if (pos >= matched.length) throw new InvalidRecordIDException();
            return rs.getRecord(matched[pos++]);
        }

        public int nextRecordId() throws InvalidRecordIDException {
            if (pos >= matched.length) throw new InvalidRecordIDException();
            return matched[pos++];
        }

        public byte[] previousRecord() throws InvalidRecordIDException, RecordStoreException {
            if (pos <= 0) throw new InvalidRecordIDException();
            return rs.getRecord(matched[--pos]);
        }

        public int previousRecordId() throws InvalidRecordIDException {
            if (pos <= 0) throw new InvalidRecordIDException();
            return matched[--pos];
        }

        public void reset() { pos = 0; }
        public int numRecords() { return matched.length; }
        public boolean isKeptUpdated() { return keepUpdated; }
        public void keepUpdated(boolean k) { }
    }

    byte[] getRecById(int id) {
        int idx = indexOf(id);
        return idx < 0 ? new byte[0] : (byte[]) recs.elementAt(idx);
    }

    /* ---- natives（Bridge 实现，Windows headless 注册文件版） ---- */
    static native byte[] nativeLoadStore(String name);
    static native void nativeSaveStore(String name, byte[] data);
    static native String[] nativeListStores();
}
