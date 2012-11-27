package org.pk;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

public class Triggercord extends Activity
{
    private static final String TAG = "Triggercord";
    protected Camera camera;
    /** Called when the activity is first created. */
    
    public void onCreate(Bundle savedInstanceState)
    {
    	super.onCreate(savedInstanceState);
    	requestWindowFeature(Window.FEATURE_NO_TITLE);
    	setContentView(R.layout.triggercord);
        camera = pentax.camera();
        TextView status = (TextView)findViewById(R.id.StatusText);
        Button trigger = (Button)findViewById(R.id.TriggerButton);
        Spinner aperture = (Spinner) findViewById(R.id.ApertureSpinner);
        ArrayAdapter<CharSequence> apertureAdapter =
            new ArrayAdapter<CharSequence>(this, android.R.layout.simple_spinner_item);
        aperture.setAdapter(apertureAdapter);
        if (camera == null)
        {
        status.setText("No camera found");
            status.append(" :( ");
        }
        else
        {
            status.setText("Found camera: ");
            status.append(camera.model());
            trigger.setEnabled(true);
        }
    }

    public void shoot(View view) {
        if (camera != null)
            camera.shoot();
    }
    
    public void focus(View view) {
        if (camera != null)
            camera.focus();
    }

    static {
        System.loadLibrary("stlport_shared");
        System.loadLibrary("pentax");
    }
}
