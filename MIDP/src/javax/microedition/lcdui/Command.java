package javax.microedition.lcdui;

public class Command {

    public static final int SCREEN = 1, BACK = 2, CANCEL = 3, OK = 4, HELP = 5,
            STOP = 6, EXIT = 7, ITEM = 8;

    private final String label;
    private final String shortLabel;
    private final int commandType;
    private final int priority;

    public Command(String label, int commandType, int priority) {
        this(label, label, commandType, priority);
    }

    public Command(String shortLabel, String label, int commandType, int priority) {
        if (label == null) throw new NullPointerException();
        this.label = label;
        this.shortLabel = (shortLabel == null) ? label : shortLabel;
        this.commandType = commandType;
        this.priority = priority;
    }

    public String getLabel() { return label; }
    public String getShortLabel() { return shortLabel; }
    public int getCommandType() { return commandType; }
    public int getPriority() { return priority; }
}
