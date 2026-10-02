package ovh.datanet.dataki.client.core;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.MultipartBody;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okio.BufferedSource;

/**
 * Streaming chat transport (Architecture §7.1/§7.2). POSTs to chat_handler.php
 * as multipart/form-data with the device-key Bearer and parses the SSE
 * response incrementally. Callbacks fire on an OkHttp background thread — the
 * caller marshals to the UI thread.
 */
public final class ChatClient {

    public static final class Params {
        public String content;
        public long chatId;      // 0 = new chat
        public String model;
        public String effort;    // may be null
        public boolean thinking = true;
        public boolean websearch = false;
    }

    public interface Listener {
        void onEvent(JSONObject event);          // background thread
        void onDone(@Nullable String error);     // background thread
    }

    /* No read timeout — streams can be long (server caps at ~300s). */
    private static final OkHttpClient STREAM_CLIENT = new OkHttpClient.Builder()
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS)
            .build();

    private Call call;

    public void send(String deviceKey, Params p, Listener listener) {
        MultipartBody.Builder mb = new MultipartBody.Builder()
                .setType(MultipartBody.FORM)
                .addFormDataPart("action", "send_message")
                .addFormDataPart("content", p.content == null ? "" : p.content)
                .addFormDataPart("chat_id", String.valueOf(p.chatId))
                .addFormDataPart("model", p.model == null ? "" : p.model)
                .addFormDataPart("thinking", p.thinking ? "1" : "0")
                .addFormDataPart("websearch", p.websearch ? "1" : "0");
        if (p.effort != null) {
            mb.addFormDataPart("effort", p.effort);
        }

        Request request = new Request.Builder()
                .url(ApiConfig.BASE_URL + "/chat_handler.php")
                .header("Authorization", "Bearer " + deviceKey)
                .header("X-Client", ApiConfig.CLIENT_HEADER)
                .header("Accept", "text/event-stream")
                .post(mb.build())
                .build();

        call = STREAM_CLIENT.newCall(request);
        call.enqueue(new Callback() {
            @Override
            public void onFailure(Call c, IOException e) {
                listener.onDone(c.isCanceled() ? null : "Netzwerkfehler");
            }

            @Override
            public void onResponse(Call c, Response response) {
                try (Response r = response) {
                    if (!r.isSuccessful()) {
                        listener.onDone("Serverfehler (HTTP " + r.code() + ").");
                        return;
                    }
                    if (r.body() == null) {
                        listener.onDone(null);
                        return;
                    }
                    SseParser parser = new SseParser(payload -> {
                        try {
                            listener.onEvent(new JSONObject(payload));
                        } catch (JSONException ignored) {
                            // skip malformed event (defensive, per §7.2)
                        }
                    });
                    BufferedSource src = r.body().source();
                    String line;
                    while ((line = src.readUtf8Line()) != null) {
                        parser.feed(line + "\n");
                    }
                    parser.flush();
                    listener.onDone(null);
                } catch (IOException e) {
                    listener.onDone(c.isCanceled() ? null : "Verbindung unterbrochen.");
                }
            }
        });
    }

    public void cancel() {
        if (call != null) {
            call.cancel();
        }
    }
}
