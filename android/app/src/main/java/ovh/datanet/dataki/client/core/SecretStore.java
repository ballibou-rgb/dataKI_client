package ovh.datanet.dataki.client.core;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.security.crypto.EncryptedSharedPreferences;
import androidx.security.crypto.MasterKey;

import java.io.IOException;
import java.security.GeneralSecurityException;

/**
 * Secure storage for the device key (docs/architecture.md §ADR-4, §11).
 *
 * The Android counterpart of libsecret / Windows Credential Manager: the key is
 * held in {@link EncryptedSharedPreferences}, backed by a key in the Android
 * Keystore. Only the device key is ever stored — never a password.
 *
 * Login is not implemented in M1; this class is the stable storage contract that
 * the login milestone (M3) builds on.
 */
public final class SecretStore {

    private static final String PREFS_FILE = "dataki_secrets";
    private static final String KEY_DEVICE = "device_key";

    private final SharedPreferences prefs;

    public SecretStore(Context context) throws GeneralSecurityException, IOException {
        Context app = context.getApplicationContext();
        MasterKey masterKey = new MasterKey.Builder(app)
                .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                .build();
        this.prefs = EncryptedSharedPreferences.create(
                app,
                PREFS_FILE,
                masterKey,
                EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM);
    }

    public void storeDeviceKey(String deviceKey) {
        prefs.edit().putString(KEY_DEVICE, deviceKey).apply();
    }

    public String getDeviceKey() {
        return prefs.getString(KEY_DEVICE, null);
    }

    public boolean hasDeviceKey() {
        return getDeviceKey() != null;
    }

    public void clear() {
        prefs.edit().remove(KEY_DEVICE).apply();
    }
}
