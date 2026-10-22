package javax.microedition.lcdui;

public interface Choice {

    int IMPLICIT = 3, EXCLUSIVE = 1, MULTIPLE = 2, POPUP = 4;

    int append(String stringPart, Image imagePart);
    void insert(int elementNum, String stringPart, Image imagePart);
    void delete(int elementNum);
    void deleteAll();
    void set(int elementNum, String stringPart, Image imagePart);
    boolean isSelected(int elementNum);
    int getSelectedIndex();
    void setSelectedIndex(int elementNum, boolean selected);
    int getSelectedFlags(boolean[] selectedArray_return);
    void setSelectedFlags(boolean[] selectedArray);
    Font getFont(int elementNum);
    void setFont(int elementNum, Font font);
    int size();
    String getString(int elementNum);
    Image getImage(int elementNum);

    int fitWidth(int elementNum, Font font);
    int getFitPolicy();
    void setFitPolicy(int fitPolicy);
    void setSelectCommand(Command command);
}
