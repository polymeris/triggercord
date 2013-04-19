package org.pk;

import java.util.Timer;
import java.util.TimerTask;
import android.app.*;
import android.content.res.Resources;
import android.content.pm.*;
import android.content.*;
import android.os.Bundle;
import android.os.Environment;
import android.net.*;
import android.util.Log;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.view.*;
import android.widget.*;

public class Triggercord extends Activity implements
    AdapterView.OnItemSelectedListener,
    CompoundButton.OnCheckedChangeListener,
    View.OnClickListener,
    SeekBar.OnSeekBarChangeListener
{
    private static final String TAG = "Triggercord";

    private Timer update;
    
    protected Camera camera;
    protected TextView mode, status;
    protected ImageView isoIcon, apertureIcon, shutterIcon, ecIcon;
    protected ImageView photo;
    private Bitmap currentImage;
    protected String filename;
    
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
    	super.onCreate(savedInstanceState);
    	setContentView(R.layout.triggercord);

        mode =          (TextView)findViewById(R.id.modeText);
        isoIcon =       (ImageView)findViewById(R.id.isoIcon);
        apertureIcon =  (ImageView)findViewById(R.id.apertureIcon);
        shutterIcon =   (ImageView)findViewById(R.id.shutterIcon);
        ecIcon =        (ImageView)findViewById(R.id.ecIcon);
        photo =         (ImageView)findViewById(R.id.photoView);
        status =        (TextView)findViewById(R.id.statusText);
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

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.triggercord, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId())
        {
            case R.id.infoMenuItem:
                String message = new String();
                try
                {
                    PackageInfo pInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
                    message += "Triggercord version: " + pInfo.versionName + "\n";
                    message += "Camera API Version: " + camera.getAPI_VERSION() + "\n";
                }
                catch (PackageManager.NameNotFoundException ex)
                {
                    message += "Unknown application version\n";
                }

                if (camera != null)
                    message += "\nCamera status\n" +
                        camera.getStatusInformation().replaceAll(" +", " ").replaceAll(" :", ":");
                else
                    message += "\nNo camera has been detected.";
                
                AlertDialog alertDialog = new AlertDialog.Builder(this).create();
                alertDialog.setTitle("Status Information");
                alertDialog.setMessage(message);
                alertDialog.setIcon(R.drawable.about);
                alertDialog.show();
                return true;
            case R.id.helpMenuItem:
                String path = getResources().getString(R.string.helpPath);
                Intent viewHelp = new Intent(Intent.ACTION_VIEW, Uri.parse(path));
                startActivity(viewHelp);
                return true;
            case R.id.shareMenuItem:
                if (filename == null)
                    return true;
                Intent share = new Intent();
                share.setAction(Intent.ACTION_SEND);
                share.putExtra(Intent.EXTRA_STREAM, filename);
                share.setType("image/jpeg");
                startActivity(Intent.createChooser(share,
                    getResources().getText(R.string.shareTitle)));
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    protected void registerChildListeners(ViewGroup vg)
    {
        for (int i = 0; i < vg.getChildCount(); i++)
        {
            View child = vg.getChildAt(i);
            if (child instanceof AdapterView)
            {
                String tag = (String)child.getTag();
                AdapterView av = (AdapterView)child;
                av.setAdapter(new ParameterAdapter(camera, tag));
                av.setOnItemSelectedListener(this);
            }
            else if (child instanceof CompoundButton)
                ((CompoundButton)child).setOnCheckedChangeListener(this);
            else if (child instanceof Button)
                ((Button)child).setOnClickListener(this);
            else if (child instanceof SeekBar)
                ((SeekBar)child).setOnSeekBarChangeListener(this);
            else if (child instanceof ViewGroup)
                this.registerChildListeners((ViewGroup)child);
        }
    }

    protected void updateChildren(ViewGroup vg)
    {
        Log.d(TAG, "updating children of " + vg.getId());
        if (camera == null)
            return;
        for (int i = 0; i < vg.getChildCount(); i++)
        {
            View child = vg.getChildAt(i);
            if (child instanceof AdapterView)
            {
                AdapterView av = (AdapterView)child;
                ((BaseAdapter)av.getAdapter()).notifyDataSetChanged();
                Log.d(TAG, "updated AdapterV: " + av.getTag() + ", " +
                    ((BaseAdapter)av.getAdapter()).getCount() + " items");
            }
            else if (child instanceof TextView)
            {
                TextView text = (TextView)child;
                String param = (String)text.getTag();
                if (param == null)
                    continue;
                String[] p = param.split(":");
                if (p.length < 2)
                    text.setText(camera.getString(p[0]));
                else if (p[0] == "String")
                    text.setText(camera.getString(p[1]));
            }
            else if (child instanceof ViewGroup)
                this.updateChildren((ViewGroup)child);
        }
    }

    public void onUpdate()
    {
        if (status == null)
            return;
        if (!loadCamera())
            return;
        updateChildren((ViewGroup)findViewById(R.id.mainFragment));
        updateChildren((ViewGroup)findViewById(R.id.settingsFragment));
    }

    public void onClick(View parent)
    {
        Log.d(TAG, "onClick(parent): parent.getID() == " + parent.getId());
        if (camera == null)
            return;

        if (parent.getId() == R.id.triggerButton)
        {
            filename = camera.shoot();
            status.setText("Shot. Saved to ");
            status.append(filename);
            BitmapFactory.Options bitmapOptions = new BitmapFactory.Options();
            bitmapOptions.inSampleSize = 4;
            currentImage = BitmapFactory.decodeFile(filename, bitmapOptions);
            photo.setImageBitmap(currentImage);
            return;
        }

        if (parent.getId() == R.id.focusButton)
            camera.focus();
    }

    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id)
    {
        Log.d(TAG, "onItemSelected(...): parent.getId() == " + parent.getId());
        if (camera == null)
            return;
        ParameterAdapter adapter = (ParameterAdapter)parent.getAdapter();
        adapter.itemSelected(pos);
    }

    public void onNothingSelected(AdapterView<?> parent)
    {
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
            camera.setString((String)button.getTag(),
                (isChecked ? button.getTextOn() : button.getTextOff()).toString());
        }
        catch (ClassCastException ex)
        {}
    }

    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser)
    {
        if (camera == null || !fromUser)
            return;
        String[] p = ((String)seekBar.getTag()).split(":");
        if (p.length < 2 || p[0] == "String")
            return;
        camera.setStopByIndex(p[1], progress);
    }
    
    public void onStartTrackingTouch(SeekBar seekBar)
    {
    }

    public void onStopTrackingTouch(SeekBar seekBar)
    {
    }

    public boolean loadCamera()
    {
        if (camera != null)
            return true;

        Log.v(TAG, "Trying to load camera.");

        camera = pentax.camera();
        if (camera == null)
        {
            status.setText("No camera found");
            return false;
        }

        Log.v(TAG, "Found camera.");
        
        status.setText("Found camera: ");
        status.append(camera.getString("Camera Model"));

        Log.v(TAG, "Configuring file destination.");
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

        Log.v(TAG, "Starting to update camera.");
        camera.startUpdating();
        
        registerChildListeners((ViewGroup)findViewById(R.id.mainFragment));
        registerChildListeners((ViewGroup)findViewById(R.id.settingsFragment));
        return true;
    }

    class ParameterAdapter extends BaseAdapter
    {
        ParameterAdapter(Camera camera, String parameter)
        {
            this.camera = camera;
            String[] p = parameter.split(":");
            if (p.length < 2)
            {
                isString = true;
                this.parameter = p[0];
            }
            else if (p[0] == "String")
            {
                isString = true;
                this.parameter = p[1];
            }
            else
            {
                isString = false;
                this.parameter = p[1];
            }
        }
        
        public int getCount()
        {
            if (camera == null)
                return 0;
            if (isString)
                return camera.getStringCount(parameter);
            else
                return camera.getStopCount(parameter);
        }

        public Object getItem(int pos)
        {
            if (camera == null)
                return null;
            if (isString)
                return camera.getStringOption(parameter, pos);
            else
                return camera.getStopOptionAsString(parameter, pos);
        }

        public long getItemId(int position)
        {
            return -1;
        }

        public View getView(int position, View convertView, ViewGroup parent)
        {
            Context context = parent.getContext();
            String item = (String)getItem(position); 

            String resStr = parameter + " " + item;
            resStr = "drawable/" + resStr.replaceAll("-", "_").replaceAll(" ", "_").toLowerCase();
            String pkg = context.getPackageName();
            int id = context.getResources().getIdentifier(resStr, "drawable", pkg);
            if (id != 0)
            {
                ImageView view;
                if (convertView != null && convertView instanceof ImageView)
                    view = (ImageView)convertView;
                else
                    view = new ImageView(context);
                AbsListView.LayoutParams layout = new AbsListView.LayoutParams(
                    AbsListView.LayoutParams.MATCH_PARENT,
                    AbsListView.LayoutParams.WRAP_CONTENT);
                view.setLayoutParams(layout);
                view.setScaleType(ImageView.ScaleType.FIT_CENTER);
                view.setPadding(0, 4, 0, 4);
                view.setImageResource(id);
                return view;
            }
            TextView view;
            if (convertView != null && convertView instanceof TextView)
                view = (TextView)convertView;
            else
                view = new TextView(context);
            view.setText(item);
            return view;
        }

        public void itemSelected(int pos)
        {
            if (isString)
                camera.setStringByIndex(parameter, pos);
            else
                camera.setStopByIndex(parameter, pos);
        }

        private Camera camera;
        private String parameter;
        boolean isString;
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
