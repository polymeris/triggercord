package org.pk;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.*;
import android.view.*;
import android.widget.*;
import android.widget.AdapterView.OnItemSelectedListener;

public class Triggercord extends Activity implements OnItemSelectedListener
{
    private static final String TAG = "Triggercord";
    protected Camera camera;
    protected TextView mode, status;
    protected Spinner iso, aperture, shutter, ec;
    protected ImageView photo;
    protected Button trigger, focus;
    /** Called when the activity is first created. */
    
    public void onCreate(Bundle savedInstanceState)
    {
    	super.onCreate(savedInstanceState);
    	requestWindowFeature(Window.FEATURE_NO_TITLE);
    	setContentView(R.layout.triggercord);
        camera = pentax.camera();
        mode =      (TextView)findViewById(R.id.ModeText);
        iso =       (Spinner)findViewById(R.id.IsoSpinner);
        aperture =  (Spinner)findViewById(R.id.ApertureSpinner);
        shutter =   (Spinner)findViewById(R.id.ShutterSpinner);
        ec =        (Spinner)findViewById(R.id.ECSpinner);
        photo =     (ImageView)findViewById(R.id.PhotoView);
        status =    (TextView)findViewById(R.id.StatusText);
        trigger =   (Button)findViewById(R.id.TriggerButton);
            
        if (camera == null)
        {
            status.setText("No camera found");
            status.append(" :( ");
            return;
        }
        else
        {
            status.setText("Found camera: ");
            status.append(camera.model());
            trigger.setEnabled(true);
        }

        ArrayAdapter<CharSequence> isoAdapter =
            new ArrayAdapter<CharSequence>(this, android.R.layout.simple_spinner_item);
        isoAdapter.add("AUTO");
        isoAdapter.add(Float.toString(camera.minimumIso()));
        for (int i = 0; i < isoValues.length; i++)
        {
            if (isoValues[i] > camera.minimumIso() &&
                isoValues[i] < camera.maximumIso())
                isoAdapter.add(Float.toString(isoValues[i]));
        }
        isoAdapter.add(Float.toString(camera.maximumIso()));
        iso.setAdapter(isoAdapter);
        
        ArrayAdapter<CharSequence> apertureAdapter =
            new ArrayAdapter<CharSequence>(this, android.R.layout.simple_spinner_item);
        apertureAdapter.add("AUTO");
        apertureAdapter.add(Float.toString(camera.minimumAperture()));
        for (int i = 0; i < apertureValues.length; i++)
        {
            if (apertureValues[i] > camera.minimumAperture() &&
                apertureValues[i] < camera.maximumAperture())
                apertureAdapter.add(Float.toString(apertureValues[i]));
        }
        apertureAdapter.add(Float.toString(camera.maximumAperture()));
        aperture.setAdapter(apertureAdapter);

        ArrayAdapter<CharSequence> shutterAdapter =
            new ArrayAdapter<CharSequence>(this, android.R.layout.simple_spinner_item);
        shutterAdapter.add("AUTO");
        for (int i = 0; i < shutterOneOverValues.length; i++)
        {
            if ((1f / shutterOneOverValues[i]) >= camera.minimumAperture())
                shutterAdapter.add(Float.toString(shutterOneOverValues[i]));
        }
        for (int i = 0; i < shutterSecondValues.length; i++)
        {
            if (shutterSecondValues[i] < camera.maximumShutter())
                shutterAdapter.add(Float.toString(shutterSecondValues[i]) + "\"");
        }
        shutterAdapter.add(Float.toString(camera.maximumShutter()) + "\"");
        shutter.setAdapter(shutterAdapter);

        ArrayAdapter<CharSequence> ecAdapter =
            new ArrayAdapter<CharSequence>(this, android.R.layout.simple_spinner_item);
        ecAdapter.add(Float.toString(camera.minimumExposureCompensation()));
        for (int i = 0; i < ecValues.length; i++)
        {
            if (apertureValues[i] > camera.minimumExposureCompensation() &&
                apertureValues[i] < camera.maximumExposureCompensation())
                ecAdapter.add(Float.toString(ecValues[i]));
        }
        ecAdapter.add(Float.toString(camera.maximumExposureCompensation()));
        ec.setAdapter(ecAdapter);

        iso.setOnItemSelectedListener(this);
        aperture.setOnItemSelectedListener(this);
        shutter.setOnItemSelectedListener(this);
        ec.setOnItemSelectedListener(this);
    }

    public void shoot(View view)
    {
        if (camera == null)
            return;
        String filename = camera.shoot();
        status.setText("Shot. Saved to ");
        status.append(filename);
        Bitmap img = BitmapFactory.decodeFile(filename);
        photo.setImageBitmap(img);
    }
    
    public void focus(View view)
    {
        if (camera != null)
            camera.focus();
    }

    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id)
    {
        String str = ((CharSequence)parent.getItemAtPosition(pos)).toString();
        switch(parent.getId())
        {
            case R.id.IsoSpinner:
                if (str == "AUTO")
                {
                    camera.setIso(-1);
                    return;
                }
                camera.setIso(Integer.parseInt(str));
                return;
            case R.id.ApertureSpinner:
                if (str == "AUTO")
                {
                    camera.setAperture(-1);
                    return;
                }
                camera.setAperture(Float.parseFloat(str));
                return;
            case R.id.ShutterSpinner:
                if (str == "AUTO")
                {
                    camera.setAperture(-1);
                    return;
                }
                if (str.endsWith("\""))
                {
                    camera.setShutter(Float.parseFloat(str.replace("\"", "")));
                    return;
                }
                camera.setShutter(1f / Float.parseFloat(str));
                return;
            case R.id.ECSpinner:
                camera.setExposureCompensation(Float.parseFloat(str));
                return;
        }
    }

    public void onNothingSelected(AdapterView<?> parent)
    {
    }


    protected final float[] apertureValues = {
        1.0f, 1.2f, 1.4f, 1.8f, 2.0f, 2.4f, 2.8f, 3.5f, 4.0f, 4.5f, 5.6f, 6.7f, 8f,
        9.5f, 11, 13, 16, 19, 22, 26, 32, 38, 44, 52, 64};

    protected final float[] shutterOneOverValues = {
        8000, 6000, 4000, 3000, 2000, 1500, 1000, 750, 500,
        350, 250, 180, 125, 90, 60, 45, 30, 20, 15, 20, 8, 6, 4, 3, 2, 1.4f, 1};

    protected final float[] shutterSecondValues = {
        1.5f, 2f, 3f, 4f, 6, 8, 10, 15, 20, 30, 40, 60};

    protected final float[] ecValues = {
        -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    protected final float[] isoValues = {
        50, 80, 100, 140, 200, 280, 400, 560, 800, 1100, 1600, 2200, 3200, 4500, 6400, 9000, 12800,
        18000, 25600, 36000, 51200, 64000, 100000, 160000 };

    static
    {
        System.loadLibrary("stlport_shared");
        System.loadLibrary("pentax");
    }
}
