package javax.microedition.rms;

import java.util.Enumeration;

public interface RecordEnumeration {
    boolean hasNextElement();
    boolean hasPreviousElement();
    byte[] nextRecord() throws InvalidRecordIDException, RecordStoreException;
    int nextRecordId() throws InvalidRecordIDException;
    byte[] previousRecord() throws InvalidRecordIDException, RecordStoreException;
    int previousRecordId() throws InvalidRecordIDException;
    boolean isKeptUpdated();
    void keepUpdated(boolean keepUpdated);
    void rebuild();
    void reset();
    int numRecords();
}
