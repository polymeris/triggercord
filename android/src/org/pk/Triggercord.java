package org.pk;

import java.util.Timer;
import java.util.TimerTask;
import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.graphics.*;
import android.view.*;
import android.widget.*;

public class Triggercord extends Activity implements
    AdapterView.OnItemSelectedListener, CompoundButton.OnCheckedChangeListener
{
    private static final String TAG = "Triggercord";

    private Timer update;
    
    protected Camera camera;
    protected TextView mode, status;
    protected ImageView isoIcon, apertureIcon, shutterIcon, ecIcon;
    protected ImageView photo;
    private Bitmap currentImage;
    
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
    	super.onCreate(savedInstanceState);
    	requestWindowFeature(Window.FEATURE_NO_TITLE);
    	setContentView(R.layout.triggercord);
        
        mode =          (TextView)findViewById(R.id.ModeText);
        isoIcon =       (ImageView)findViewById(R.id.IsoIcon);
        apertureIcon =  (ImageView)findViewById(R.id.ApertureIcon);
        shutterIcon =   (ImageView)findViewById(R.id.ShutterIcon);
        ecIcon =        (ImageView)findViewById(R.id.ECIcon);
        photo =         (ImageView)findViewById(R.id.PhotoView);
        status =        (TextView)findViewById(R.id.StatusText);
        Display disp = getWindowManager().getDefaultDisplay();
    }

    @Override
    public void onResume()
    {
        super.onResume();

        TimerTask updateTask = new TimerTask()
        {
            @Override
            public void run()
            {
                runOnUiThread(new Runnable() { public void run() { update(); } });
            }
        };
        update = new Timer();
        update.schedule(updateTask, 0, 1000);
    }

    @Override
    public void onPause()
    {
        super.onPause();
        if (update != null)
            update.cancel();
    }

    public void update()
    {
        if (!loadCamera())
            return;
    }

    public void on(View view)
    {
        if (camera == null)
            return;
        String filename = camera.shoot();
        status.setText("Shot saved to ");
        status.append(filename);
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inSampleSize = 4;
        currentImage = BitmapFactory.decodeFile(filename, opts);
        photo.setImageBitmap(currentImage);
    }

    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id)
    {
    }

    public void onNothingSelected(AdapterView<?> parent)
    {
    }

    public void onCheckedChanged(CompoundButton parent, boolean isChecked)
    {
        if (camera == null)
            return;
        ToggleButton button = (ToggleButton)parent;
        camera.set(button.getTag(), isChecked ? button.getOnText() : button.getOffText());
    }

        public boolean loadCamera()
    {
        if (camera != null)
            return true;

        camera = pentax.camera();
        if (camera == null)
        {
            status.setText("No camera found");
            return false;
        }
        
        status.setText("Found camera: ");
        status.append(camera.model());
        java.io.File picDir;
        picDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES);
        picDir.mkdirs();
        if (!picDir.exists())
        {
            picDir = new java.io.File("/sdcard/Pictures");
            picDir.mkdirs();
        }
        if (!picDir.exists())
            status.setText("Can't access storage.");
        camera.setFileDestination(picDir.getAbsolutePath());

        return true;
    }

    static
    {
        try
        {
            System.loadLibrary("stlport_shared");
        }
        catch (UnsatisfiedLinkError err)
        {
            System.loadLibrary("stlport");
        }
        System.loadLibrary("pentax");
    }
}
