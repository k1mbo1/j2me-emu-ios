package javax.microedition.rms;

public interface RecordListener {
    void recordAdded(RecordStore rs, int recordId);
    void recordChanged(RecordStore rs, int recordId);
    void recordDeleted(RecordStore rs, int recordId);
}
