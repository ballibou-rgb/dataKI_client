package ovh.datanet.dataki.client.ui;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;

import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.recyclerview.widget.LinearLayoutManager;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import ovh.datanet.dataki.client.R;
import ovh.datanet.dataki.client.core.ApiConfig;
import ovh.datanet.dataki.client.core.AuthManager;
import ovh.datanet.dataki.client.core.ChatClient;
import ovh.datanet.dataki.client.core.SecretStore;
import ovh.datanet.dataki.client.databinding.ActivityMainBinding;

/**
 * Login + chat (Android M3/M4). The browser hand-off uses an https App-Link
 * redirect; once signed in the chat streams over chat_handler.php with the
 * device-key Bearer.
 */
public class MainActivity extends AppCompatActivity {

    private static final String PREFS = "dataki_login";
    private static final String KEY_STATE = "state";

    private ActivityMainBinding binding;
    private SecretStore store;
    private AuthManager auth;
    private SharedPreferences loginPrefs;

    // chat state
    private MessageAdapter adapter;
    private ChatClient chat;
    private final List<String> modelIds = new ArrayList<>();
    private String effortDefault = "mittel";
    private boolean isProxy = true;
    private boolean streaming = false;
    private int assistantPos = -1;
    private long currentChatId = 0;

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

        adapter = new MessageAdapter();
        binding.messagesRecycler.setLayoutManager(new LinearLayoutManager(this));
        binding.messagesRecycler.setAdapter(adapter);

        binding.signInButton.setOnClickListener(v -> startLogin());
        binding.signOutButton.setOnClickListener(v -> {
            if (streaming && chat != null) chat.cancel();
            if (auth != null) auth.logout();
            adapter.clear();
            currentChatId = 0;
            showLogin();
        });
        binding.sendButton.setOnClickListener(v -> onSendOrStop());

        if (auth != null && auth.hasKey()) {
            bootstrapAndShow();
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

    /* ---- login ---- */

    private void startLogin() {
        if (auth == null) {
            showError("Kein sicherer Speicher verfügbar.");
            return;
        }
        String state = UUID.randomUUID().toString();
        loginPrefs.edit().putString(KEY_STATE, state).apply();
        showError("");
        CustomTabsIntent tab = new CustomTabsIntent.Builder().build();
        tab.launchUrl(this, Uri.parse(auth.buildLoginUrl(state)));
    }

    private void handleRedirect(Intent intent) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) return;
        Uri data = intent.getData();
        if (data == null) return;
        String code = data.getQueryParameter("code");
        String state = data.getQueryParameter("state");
        if (code == null) return;

        String expected = loginPrefs.getString(KEY_STATE, null);
        if (expected == null || !expected.equals(state)) {
            showError(getString(R.string.login_failed));
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
                parseBootstrap(json);
                showChat();
            } else {
                if (status == 401 && auth != null) auth.logout();
                showError(getString(R.string.login_failed));
            }
        }));
    }

    private void parseBootstrap(String json) {
        if (json == null) return;
        try {
            JSONObject o = new JSONObject(json);
            isProxy = "proxy".equals(o.optString("chat_mode", "proxy"));
            effortDefault = o.optString("effort_default", "mittel");

            modelIds.clear();
            List<String> labels = new ArrayList<>();
            JSONArray models = o.optJSONArray("models");
            if (models != null) {
                for (int i = 0; i < models.length(); i++) {
                    JSONObject m = models.getJSONObject(i);
                    String id = m.optString("id", "");
                    if (!id.isEmpty()) {
                        modelIds.add(id);
                        labels.add(m.optString("label", id));
                    }
                }
            }
            ArrayAdapter<String> sa = new ArrayAdapter<>(this,
                    android.R.layout.simple_spinner_item, labels);
            sa.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            binding.modelSpinner.setAdapter(sa);

            if (isProxy) {
                JSONObject ts = o.optJSONObject("token_status");
                if (ts != null) binding.tokenText.setText(ts.optString("tier", ""));
            }
        } catch (Exception ignored) {
        }
    }

    /* ---- chat ---- */

    private void onSendOrStop() {
        if (streaming) {
            if (chat != null) chat.cancel();
            return;
        }
        String text = binding.inputText.getText() != null
                ? binding.inputText.getText().toString().trim() : "";
        if (text.isEmpty() || modelIds.isEmpty()) return;

        int sel = binding.modelSpinner.getSelectedItemPosition();
        if (sel < 0 || sel >= modelIds.size()) sel = 0;
        String model = modelIds.get(sel);

        binding.inputText.setText("");
        adapter.add(new Message(Message.ROLE_USER, text));
        assistantPos = adapter.add(new Message(Message.ROLE_ASSISTANT, ""));
        scrollToEnd();

        ChatClient.Params p = new ChatClient.Params();
        p.content = text;
        p.chatId = currentChatId;
        p.model = model;
        p.effort = effortDefault;

        chat = new ChatClient();
        setStreaming(true);
        chat.send(store.getDeviceKey(), p, new ChatClient.Listener() {
            @Override public void onEvent(JSONObject event) {
                runOnUiThread(() -> handleChatEvent(event));
            }
            @Override public void onDone(String error) {
                runOnUiThread(() -> {
                    if (error != null) appendAssistant("⚠ " + error);
                    setStreaming(false);
                });
            }
        });
    }

    private void handleChatEvent(JSONObject e) {
        String type = e.optString("type", "");
        switch (type) {
            case "content_delta":
                appendAssistant(e.optString("delta", ""));
                break;
            case "chat_id":
                currentChatId = e.optLong("chat_id", currentChatId);
                break;
            case "error":
                appendAssistant("⚠ " + e.optString("message", "Fehler"));
                break;
            case "done":
                if (isProxy) {
                    JSONObject ts = e.optJSONObject("token_status");
                    if (ts != null) {
                        binding.tokenText.setText(ts.optLong("tier_used") + " / " + ts.optLong("limit"));
                    }
                }
                break;
            default:
                // thinking_delta / tool_status / sources / attachment: ignored in M4 MVP
                break;
        }
    }

    private void appendAssistant(String delta) {
        if (assistantPos < 0 || delta.isEmpty()) return;
        adapter.get(assistantPos).content.append(delta);
        adapter.changed(assistantPos);
        scrollToEnd();
    }

    private void scrollToEnd() {
        if (adapter.getItemCount() > 0) {
            binding.messagesRecycler.scrollToPosition(adapter.getItemCount() - 1);
        }
    }

    private void setStreaming(boolean on) {
        streaming = on;
        binding.sendButton.setText(on ? R.string.stop : R.string.send);
        binding.inputText.setEnabled(!on);
    }

    /* ---- view state ---- */

    private void showLogin()  { binding.viewFlipper.setDisplayedChild(0); }
    private void showChat()   { binding.viewFlipper.setDisplayedChild(1); }
    private void setBusy(boolean b) {
        binding.loginProgress.setVisibility(b ? View.VISIBLE : View.GONE);
        binding.signInButton.setEnabled(!b);
    }
    private void showError(String m) { binding.loginError.setText(m); }
}
