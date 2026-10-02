package ovh.datanet.dataki.client.ui;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsIntent;

import java.util.UUID;

import ovh.datanet.dataki.client.R;
import ovh.datanet.dataki.client.core.AuthManager;
import ovh.datanet.dataki.client.core.SecretStore;
import ovh.datanet.dataki.client.databinding.ActivityMainBinding;

/**
 * Login + signed-in shell (Android M3). The browser hand-off uses an https
 * App-Link redirect (see AndroidManifest); the chat UI follows in Android M4.
 * Declared singleTask, so the redirect re-enters via onNewIntent.
 */
public class MainActivity extends AppCompatActivity {

    private static final String PREFS = "dataki_login";
    private static final String KEY_STATE = "state";

    private ActivityMainBinding binding;
    private SecretStore store;
    private AuthManager auth;
    private SharedPreferences loginPrefs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        loginPrefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        try {
            store = new SecretStore(this);
            auth = new AuthManager(store);
        } catch (Exception e) {
            store = null;
            auth = null;
        }

        binding.signInButton.setOnClickListener(v -> startLogin());
        binding.signOutButton.setOnClickListener(v -> {
            if (auth != null) auth.logout();
            showLogin();
        });

        if (auth != null && auth.hasKey()) {
            showChat();
        } else {
            showLogin();
        }

        handleRedirect(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleRedirect(intent);
    }

    private void startLogin() {
        if (auth == null) {
            showError("Kein sicherer Speicher verfügbar.");
            return;
        }
        String state = UUID.randomUUID().toString();
        loginPrefs.edit().putString(KEY_STATE, state).apply();
        showError("");

        String url = auth.buildLoginUrl(state);
        CustomTabsIntent tab = new CustomTabsIntent.Builder().build();
        tab.launchUrl(this, Uri.parse(url));
    }

    private void handleRedirect(Intent intent) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) {
            return;
        }
        Uri data = intent.getData();
        if (data == null) {
            return;
        }
        String code = data.getQueryParameter("code");
        String state = data.getQueryParameter("state");
        if (code == null) {
            return;
        }

        String expected = loginPrefs.getString(KEY_STATE, null);
        if (expected == null || !expected.equals(state)) {
            showError(getString(R.string.login_failed)); // CSRF / state mismatch
            return;
        }
        loginPrefs.edit().remove(KEY_STATE).apply();

        setBusy(true);
        auth.exchange(code, (ok, error) -> runOnUiThread(() -> {
            if (ok) {
                bootstrapAndShow();
            } else {
                setBusy(false);
                showError(error != null ? error : getString(R.string.login_failed));
            }
        }));
    }

    private void bootstrapAndShow() {
        auth.bootstrap((ok, status, json) -> runOnUiThread(() -> {
            setBusy(false);
            if (ok) {
                // M4: parse models / token status / chat list from json.
                showChat();
            } else {
                if (status == 401 && auth != null) auth.logout();
                showError(getString(R.string.login_failed));
            }
        }));
    }

    private void showLogin() {
        binding.viewFlipper.setDisplayedChild(0);
    }

    private void showChat() {
        binding.viewFlipper.setDisplayedChild(1);
    }

    private void setBusy(boolean busy) {
        binding.loginProgress.setVisibility(busy ? View.VISIBLE : View.GONE);
        binding.signInButton.setEnabled(!busy);
    }

    private void showError(String message) {
        binding.loginError.setText(message);
    }
}
