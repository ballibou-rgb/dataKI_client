package ovh.datanet.dataki.client.ui;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

import androidx.appcompat.app.AppCompatActivity;

import ovh.datanet.dataki.client.R;
import ovh.datanet.dataki.client.databinding.ActivityMainBinding;

/**
 * Main screen (M1 shell). Shows the app bar and placeholders; login and chat
 * arrive in later milestones. Declared singleTask so the App-Links login
 * redirect re-enters here via {@link #onNewIntent(Intent)}.
 */
public class MainActivity extends AppCompatActivity {

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        setSupportActionBar(binding.toolbar);

        handleLoginRedirect(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleLoginRedirect(intent);
    }

    /**
     * Placeholder for the browser-hand-off result. In M3 this extracts the
     * one-time {@code code} and {@code state}, validates the state and exchanges
     * the code for a device key. For now it only acknowledges the redirect.
     */
    private void handleLoginRedirect(Intent intent) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) {
            return;
        }
        Uri data = intent.getData();
        if (data == null) {
            return;
        }
        String code = data.getQueryParameter("code");
        if (code != null) {
            // M3: validate state, POST login/exchange, store device key.
            binding.subtitle.setText(R.string.login_received_placeholder);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_about) {
            new androidx.appcompat.app.AlertDialog.Builder(this)
                    .setTitle(R.string.about_title)
                    .setMessage(getString(R.string.about_message))
                    .setPositiveButton(android.R.string.ok, null)
                    .show();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
