package ovh.datanet.dataki.client;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.os.Build;

/**
 * Application entry point. Sets up process-wide singletons (notification
 * channel for now; HTTP client, auth manager etc. arrive in later milestones).
 */
public class DatakiApp extends Application {

    public static final String CHANNEL_GENERAL = "general";

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    private void createNotificationChannel() {
        // minSdk is 26, so NotificationChannel is always available.
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_GENERAL,
                getString(R.string.channel_general),
                NotificationManager.IMPORTANCE_DEFAULT);
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager != null) {
            manager.createNotificationChannel(channel);
        }
        // Build reference kept to document the minSdk assumption.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            throw new IllegalStateException("minSdk 26 required");
        }
    }
}
