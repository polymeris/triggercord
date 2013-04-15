package org.pk;

import java.util.Timer;
import java.util.TimerTask;
import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.graphics.*;
import android.view.*;
import android.widget.*;

public class Triggercord extends Activity implements
    AdapterView.OnItemSelectedListener,
    CompoundButton.OnCheckedChangeListener,
    View.OnClickListener
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

        mode =          (TextView)findViewById(R.id.modeText);
        isoIcon =       (ImageView)findViewById(R.id.isoIcon);
        apertureIcon =  (ImageView)findViewById(R.id.apertureIcon);
        shutterIcon =   (ImageView)findViewById(R.id.shutterIcon);
        ecIcon =        (ImageView)findViewById(R.id.ecIcon);
        photo =         (ImageView)findViewById(R.id.photoView);
        status =        (TextView)findViewById(R.id.statusText);
        Display disp = getWindowManager().getDefaultDisplay();

        registerChildListeners((ViewGroup)findViewById(R.id.mainFragment));
        registerChildListeners((ViewGroup)findViewById(R.id.settingsFragment));
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
                runOnUiThread(new Runnable() { public void run() { onUpdate(); } });
            }
        };
        update = new Timer();
        update.schedule(updateTask, 0, 1000);
    }

    @Override
    public void onPause()
    {
        super.onPause();
        if (camera != null)
            camera.stopUpdating();
    }

    protected void registerChildListeners(ViewGroup vg)
    {
        for (int i = 0; i < vg.getChildCount(); i++)
        {
            View child = vg.getChildAt(i);
            if (child instanceof AdapterView)
                ((AdapterView)child).setOnItemSelectedListener(this);
            else if (child instanceof CompoundButton)
                ((CompoundButton)child).setOnCheckedChangeListener(this);
            else if (child instanceof Button)
                ((Button)child).setOnClickListener(this);
            else if (child instanceof ViewGroup)
                this.registerChildListeners((ViewGroup)child);
        }
    }

    public void onUpdate()
    {
        if (!loadCamera())
            return;
    }

    //~ public void on(View view)
    //~ {
        //~ if (camera == null)
            //~ return;
        //~ String filename = camera.shoot();
        //~ status.setText("Shot saved to ");
        //~ status.append(filename);
        //~ BitmapFactory.Options opts = new BitmapFactory.Options();
        //~ opts.inSampleSize = 4;
        //~ currentImage = BitmapFactory.decodeFile(filename, opts);
        //~ photo.setImageBitmap(currentImage);
    //~ }

    public void onClick(View parent)
    {
        Log.d(TAG, "onClick(parent): parent.getID() == " + parent.getId());
        if (camera == null)
            return;
    }

    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id)
    {
        Log.d(TAG, "onItemSelected(...): parent.getId() == " + parent.getId());
        if (camera == null)
            return;
    }

    public void onNothingSelected(AdapterView<?> parent)
    {
        Log.d(TAG, "onNothingSelected(parent): parent.getId() == " + parent.getId());
        if (camera == null)
            return;
    }

    public void onCheckedChanged(CompoundButton parent, boolean isChecked)
    {
        Log.d(TAG, "onCheckedChanged(parent, isChecked): parent.getId() == " + parent.getId());
        Log.d(TAG, "    isChecked == " + isChecked);
        if (camera == null)
            return;
        try
        {
            ToggleButton button = (ToggleButton)parent;
            Log.v(TAG, "    button.getTag() == " + (String)button.getTag());
            camera.set((String)button.getTag(),
                (isChecked ? button.getTextOn() : button.getTextOff()).toString());
        }
        catch (ClassCastException ex)
        {}
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
        status.append(camera.getString("Camera Model"));
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
        camera.set("File Destination", picDir.getAbsolutePath());

        camera.startUpdating();
        
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
