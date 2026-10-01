package ovh.datanet.dataki.client.core;

/**
 * Fixed backend endpoints (docs/architecture.md §1.1, §6, §7).
 * The server URL is fixed to ai.datanet.ovh in v1.
 */
public final class ApiConfig {

    private ApiConfig() {}

    public static final String BASE_URL = "https://ai.datanet.ovh";

    // /api/v1/client/*
    public static final String INFO          = BASE_URL + "/api/v1/client/info";
    public static final String LOGIN_START   = BASE_URL + "/api/v1/client/login/start";
    public static final String LOGIN_EXCHANGE = BASE_URL + "/api/v1/client/login/exchange";
    public static final String LOGOUT        = BASE_URL + "/api/v1/client/logout";
    public static final String BOOTSTRAP     = BASE_URL + "/api/v1/client/bootstrap";

    // App-Links login redirect (see AndroidManifest intent-filter).
    public static final String LOGIN_REDIRECT_URI = BASE_URL + "/app/login/callback";

    public static final String CLIENT_HEADER = "dataKI-native-android/0.1.0";
    public static final int    CLIENT_API_LEVEL = 1;
}
