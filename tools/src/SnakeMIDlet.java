import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.midlet.MIDlet;

public class SnakeMIDlet extends MIDlet implements CommandListener {

    private SnakeCanvas canvas;
    private Command exitCmd;

    public SnakeMIDlet() {
        canvas = new SnakeCanvas();
        exitCmd = new Command("退出", Command.EXIT, 1);
        canvas.addCommand(exitCmd);
        canvas.setCommandListener(this);
    }

    protected void startApp() {
        Display.getDisplay(this).setCurrent(canvas);
        canvas.start();
    }

    protected void pauseApp() {
        canvas.pause();
    }

    protected void destroyApp(boolean unconditional) {
        canvas.pause();
        canvas.saveBest();
    }

    public void commandAction(Command c, Displayable d) {
        if (c == exitCmd) {
            canvas.saveBest();
            notifyDestroyed();
        }
    }
}
