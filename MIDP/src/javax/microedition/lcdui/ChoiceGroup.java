package javax.microedition.lcdui;

import java.util.Vector;

public class ChoiceGroup extends Item implements Choice {

    private final Vector strings = new Vector();
    private final Vector images = new Vector();
    private final Vector selected = new Vector();
    private int type;
    private int cursor;
    private int fitPolicy;
    private Command selectCommand;

    public ChoiceGroup(String label, int choiceType) {
        super(label);
        if (choiceType != EXCLUSIVE && choiceType != MULTIPLE && choiceType != POPUP)
            throw new IllegalArgumentException();
        this.type = choiceType;
    }

    public ChoiceGroup(String label, int choiceType, String[] stringElements, Image[] imageElements) {
        this(label, choiceType);
        for (int i = 0; i < stringElements.length; i++)
            append(stringElements[i], imageElements == null ? null : imageElements[i]);
    }

    public int append(String stringPart, Image imagePart) {
        strings.addElement(stringPart);
        images.addElement(imagePart);
        selected.addElement(Boolean.FALSE);
        return strings.size() - 1;
    }

    public void insert(int elementNum, String stringPart, Image imagePart) {
        if (elementNum < 0 || elementNum > strings.size()) throw new IndexOutOfBoundsException();
        strings.insertElementAt(stringPart, elementNum);
        images.insertElementAt(imagePart, elementNum);
        selected.insertElementAt(Boolean.FALSE, elementNum);
    }

    public void delete(int elementNum) {
        if (elementNum < 0 || elementNum >= strings.size()) throw new IndexOutOfBoundsException();
        strings.removeElementAt(elementNum);
        images.removeElementAt(elementNum);
        selected.removeElementAt(elementNum);
        if (cursor >= strings.size()) cursor = strings.size() - 1;
    }

    public void deleteAll() {
        strings.removeAllElements();
        images.removeAllElements();
        selected.removeAllElements();
        cursor = 0;
    }

    public void set(int elementNum, String stringPart, Image imagePart) {
        if (elementNum < 0 || elementNum >= strings.size()) throw new IndexOutOfBoundsException();
        strings.setElementAt(stringPart, elementNum);
        images.setElementAt(imagePart, elementNum);
    }

    public boolean isSelected(int elementNum) {
        if (elementNum < 0 || elementNum >= strings.size()) throw new IndexOutOfBoundsException();
        return ((Boolean) selected.elementAt(elementNum)).booleanValue();
    }

    public int getSelectedIndex() {
        if (type == MULTIPLE) return -1;
        for (int i = 0; i < strings.size(); i++)
            if (((Boolean) selected.elementAt(i)).booleanValue()) return i;
        return -1;
    }

    public void setSelectedIndex(int elementNum, boolean sel) {
        if (elementNum < 0 || elementNum >= strings.size()) throw new IndexOutOfBoundsException();
        if (type == MULTIPLE) {
            selected.setElementAt(sel ? Boolean.TRUE : Boolean.FALSE, elementNum);
        } else if (sel) {
            for (int i = 0; i < strings.size(); i++)
                selected.setElementAt(i == elementNum ? Boolean.TRUE : Boolean.FALSE, i);
        }
    }

    public int getSelectedFlags(boolean[] ret) {
        if (ret == null || ret.length < strings.size()) throw new IllegalArgumentException();
        int c = 0;
        for (int i = 0; i < strings.size(); i++) {
            ret[i] = ((Boolean) selected.elementAt(i)).booleanValue();
            if (ret[i]) c++;
        }
        return c;
    }

    public void setSelectedFlags(boolean[] arr) {
        if (arr == null || arr.length < strings.size()) throw new IllegalArgumentException();
        for (int i = 0; i < strings.size(); i++)
            if (arr[i]) { setSelectedIndex(i, true); return; }
    }

    public Font getFont(int elementNum) { return Font.getDefaultFont(); }
    public void setFont(int elementNum, Font font) { }
    public int size() { return strings.size(); }

    public String getString(int elementNum) {
        if (elementNum < 0 || elementNum >= strings.size()) throw new IndexOutOfBoundsException();
        return (String) strings.elementAt(elementNum);
    }

    public Image getImage(int elementNum) {
        if (elementNum < 0 || elementNum >= strings.size()) throw new IndexOutOfBoundsException();
        return (Image) images.elementAt(elementNum);
    }

    public int fitWidth(int elementNum, Font font) { return 0; }
    public int getFitPolicy() { return fitPolicy; }
    public void setFitPolicy(int p) { fitPolicy = p; }
    public void setSelectCommand(Command command) { selectCommand = command; }

    public int getChoiceType() { return type; }

    /* ---- 渲染 ---- */
    int contentHeight(Form f) {
        Font font = Font.getDefaultFont();
        int lineH = font.getHeight() + 6;
        int h = (label != null && label.length() > 0 ? font.getHeight() + 2 : 0)
                + strings.size() * lineH + 4;
        return h;
    }

    void paintContent(Graphics g, int x, int y, int w, Form f) {
        Font font = Font.getDefaultFont();
        if (label != null && label.length() > 0) {
            g.setColor(0);
            g.drawString(label, x, y, Graphics.LEFT | Graphics.TOP);
            y += font.getHeight() + 2;
        }
        int lineH = font.getHeight() + 6;
        for (int i = 0; i < strings.size(); i++) {
            int iy = y + i * lineH;
            boolean sel = ((Boolean) selected.elementAt(i)).booleanValue();
            if (i == cursor && f.focus == this) {
                g.setColor(0x3050A0);
                g.fillRect(x + 2, iy + 1, w - 8, lineH - 2);
                g.setColor(0xFFFFFF);
            } else {
                g.setColor(0);
            }
            int tx = x + 6;
            if (type == MULTIPLE) {
                g.drawRect(x + 6, iy + lineH / 2 - 5, 10, 10);
                if (sel) g.fillRect(x + 8, iy + lineH / 2 - 3, 7, 7);
                tx = x + 24;
            } else {
                g.drawArc(x + 6, iy + lineH / 2 - 5, 10, 10, 0, 360);
                if (sel) g.fillArc(x + 8, iy + lineH / 2 - 3, 7, 7, 0, 360);
                tx = x + 24;
            }
            String s = (String) strings.elementAt(i);
            if (s != null) g.drawString(s, tx, iy + 3, Graphics.LEFT | Graphics.TOP);
        }
        g.setColor(0);
    }

    boolean keyInput(int keyCode) {
        int n = strings.size();
        if (n == 0) return false;
        if (keyCode == Canvas.KEY_NUM8 || keyCode == Canvas.DOWN) {
            if (cursor < n - 1) cursor++;
            return true;
        }
        if (keyCode == Canvas.KEY_NUM2 || keyCode == Canvas.UP) {
            if (cursor > 0) cursor--;
            return true;
        }
        if (keyCode == Canvas.KEY_NUM5 || keyCode == Canvas.FIRE) {
            if (type == MULTIPLE) {
                boolean sel = !((Boolean) selected.elementAt(cursor)).booleanValue();
                selected.setElementAt(sel ? Boolean.TRUE : Boolean.FALSE, cursor);
            } else {
                setSelectedIndex(cursor, true);
            }
            return true;
        }
        return false;
    }
}
