package ovh.datanet.dataki.client.core;

import androidx.annotation.Nullable;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;

/**
 * Thin OkHttp wrapper for the JSON endpoints (login/exchange, bootstrap,
 * logout). Callbacks fire on an OkHttp background thread — the caller marshals
 * back to the UI thread.
 */
public final class HttpClient {

    private HttpClient() {}

    public interface ResultCallback {
        void onResult(boolean ok, int status, @Nullable String body);
    }

    private static final MediaType JSON = MediaType.parse("application/json; charset=utf-8");

    private static final OkHttpClient CLIENT = new OkHttpClient.Builder()
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .build();

    private static Request.Builder base(String url, @Nullable String bearer) {
        Request.Builder b = new Request.Builder()
                .url(url)
                .header("X-Client", ApiConfig.CLIENT_HEADER)
                .header("X-Client-Api", String.valueOf(ApiConfig.CLIENT_API_LEVEL))
                .header("Accept", "application/json");
        if (bearer != null && !bearer.isEmpty()) {
            b.header("Authorization", "Bearer " + bearer);
        }
        return b;
    }

    public static void getJson(String url, @Nullable String bearer, ResultCallback cb) {
        enqueue(base(url, bearer).get().build(), cb);
    }

    public static void postJson(String url, @Nullable String bearer, String json, ResultCallback cb) {
        RequestBody body = RequestBody.create(json, JSON);
        enqueue(base(url, bearer).post(body).build(), cb);
    }

    private static void enqueue(Request request, ResultCallback cb) {
        CLIENT.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                cb.onResult(false, 0, null);
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                try (Response r = response) {
                    String body = r.body() != null ? r.body().string() : null;
                    cb.onResult(true, r.code(), body);
                }
            }
        });
    }
}
