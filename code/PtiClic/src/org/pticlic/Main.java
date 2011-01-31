package org.pticlic;

import org.pticlic.games.BaseGame;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

public class Main extends Activity implements OnClickListener {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		// Écoute des clics sur les différents boutons
		((Button)findViewById(R.id.prefs)).setOnClickListener(this);
		((Button)findViewById(R.id.play)).setOnClickListener(this);

		// On récupère le nom du joueur des préférences.
		SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
		String loginPref = sp.getString("login", "joueur");
		// On l'ajoute dans le TextView prévu à cet effet
		((TextView)findViewById(R.id.login)).setText("Login : " + loginPref);
	}

	/* (non-Javadoc)
	 * @see android.view.View.OnClickListener#onClick(android.view.View)
	 */
	@Override
	public void onClick(View v) {
		switch (v.getId()) {
		case (R.id.prefs) : startActivity(new Intent(this, Preference.class)); break;
		case (R.id.play) : startActivity(new Intent(this, BaseGame.class)); break;
		}
		if (v.getId()==R.id.voirscore){
	        // TODO: à supprimer
	        /*
	        Intent intent = new Intent(this, Score.class);
	        intent.putExtra("corrects", 10);
	        intent.putExtra("mauvais", 2);
	        intent.putExtra("manquants", 1.5);
	        intent.putExtra("total", 10); // WHERE??
	        startActivity(intent);
	        */
		}
	}
	
	
}