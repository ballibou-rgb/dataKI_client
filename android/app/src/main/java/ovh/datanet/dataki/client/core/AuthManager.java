package ovh.datanet.dataki.client.core;

import android.net.Uri;
import android.os.Build;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Drives the browser hand-off login on Android (Architecture §5):
 * build the login/start URL, exchange the one-time code for a device key,
 * store it, and bootstrap. Mirrors the validated desktop flow; the only
 * difference is the App-Link redirect instead of a loopback.
 */
public final class AuthManager {

    private final SecretStore store;

    public AuthManager(SecretStore store) {
        this.store = store;
    }

    public boolean hasKey() {
        return store.hasDeviceKey();
    }

    public static String deviceName() {
        return (Build.MANUFACTURER + " " + Build.MODEL).trim();
    }

    /** URL to open in a Custom Tab; the server redirects back via the App-Link. */
    public String buildLoginUrl(String state) {
        return Uri.parse(ApiConfig.LOGIN_START).buildUpon()
                .appendQueryParameter("redirect_uri", ApiConfig.LOGIN_REDIRECT_URI)
                .appendQueryParameter("state", state)
                .appendQueryParameter("device_name", deviceName())
                .appendQueryParameter("client_version", "0.1.0")
                .build().toString();
    }

    public interface ExchangeCallback {
        void onDone(boolean ok, @Nullable String error);
    }

    /** Exchange the one-time code for a device key (stored on success). */
    public void exchange(String code, ExchangeCallback cb) {
        JSONObject body = new JSONObject();
        try {
            body.put("code", code);
            body.put("device_name", deviceName());
            body.put("client_version", "0.1.0");
        } catch (JSONException e) {
            cb.onDone(false, "Interner Fehler");
            return;
        }

        HttpClient.postJson(ApiConfig.LOGIN_EXCHANGE, null, body.toString(), (ok, status, resp) -> {
            if (!ok || resp == null) {
                cb.onDone(false, "Netzwerkfehler");
                return;
            }
            try {
                JSONObject o = new JSONObject(resp);
                if (status == 200 && "ok".equals(o.optString("status")) && o.has("device_key")) {
                    store.storeDeviceKey(o.getString("device_key"));
                    cb.onDone(true, null);
                } else {
                    cb.onDone(false, o.optString("error", "Anmeldung fehlgeschlagen"));
                }
            } catch (JSONException e) {
                cb.onDone(false, "Ungültige Antwort vom Server");
            }
        });
    }

    public interface BootstrapCallback {
        void onDone(boolean ok, int status, @Nullable String json);
    }

    public void bootstrap(BootstrapCallback cb) {
        HttpClient.getJson(ApiConfig.BOOTSTRAP, store.getDeviceKey(),
                (ok, status, body) -> cb.onDone(ok && status == 200, status, body));
    }

    /** Revoke the device key server-side, then forget it locally. */
    public void logout() {
        String key = store.getDeviceKey();
        if (key != null) {
            HttpClient.postJson(ApiConfig.LOGOUT, key, "{}", (ok, status, body) -> { });
        }
        store.clear();
    }
}
