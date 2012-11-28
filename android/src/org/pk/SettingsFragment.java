package org.pk;

import android.app.Fragment;
import android.location.Address;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;

public class SettingsFragment extends Fragment {
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
    	setHasOptionsMenu(true);
        return inflater.inflate(R.layout.settings, container, false);
    }
    
    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
    	menu.add("Show");
    	super.onCreateOptionsMenu(menu, inflater);
    }
}