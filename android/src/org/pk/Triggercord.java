package org.pk;

import java.util.ArrayList;
import android.app.Activity;
import android.os.Bundle;
import android.widget.*;
import android.view.View;
import org.pk.*;

public class Triggercord extends Activity
{
    private static final String TAG = "Triggercord";
    protected Camera camera;
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        camera = pentax.camera();
        setContentView(R.layout.main);
        TextView status = (TextView)findViewById(R.id.StatusText);
        Button trigger = (Button)findViewById(R.id.TriggerButton);
        Spinner aperture = (Spinner) findViewById(R.id.ApertureSpinner);
        ArrayAdapter<CharSequence> apertureAdapter =
            new ArrayAdapter(this, android.R.layout.simple_spinner_item,
                new ArrayList<String>() {{
                    add("2.0");
                    add("2.8");
                    add("4.0");
                    add("5.6");
                    add("8.0");
                    }}
            );
        aperture.setAdapter(apertureAdapter);
        if (camera == null)
        {
            status.setText("No camera found");
            status.append(" :( ");
        }
        else
        {
            status.setText("Camera found!");
            status.append(camera.model());
            trigger.setEnabled(true);
        }
    }

    public void shoot(View view) {
        if (camera != null)
            camera.shoot();
    }

    static {
        System.loadLibrary("stlport_shared");
        System.loadLibrary("pentax");
    }
}
