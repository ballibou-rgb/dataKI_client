# dataKI Android client

Java Android app, part of the dataKI client family. Shares the backend contract
(`/api/v1/client/*`, Bearer device key, SSE) with the desktop clients.

- **applicationId:** `ovh.datanet.dataki.client`
- **minSdk:** 26 (Android 8) · **targetSdk/compileSdk:** 34
- **UI:** Java + Views (Material 3)
- **Build:** Gradle wrapper (Gradle 8.9, AGP 8.5.2)

## Open / build

Open the `android/` folder in Android Studio (it provides the Android SDK and
JDK), then Run. From the command line, with an Android SDK available:

```sh
cd android
./gradlew assembleDebug        # build debug APK
./gradlew test                 # run JVM unit tests (SseParserTest)
```

Android Studio creates `local.properties` with your `sdk.dir` on first open
(it is git-ignored).

## Status — Milestone 1

Runnable shell only: app bar, placeholders, encrypted key storage stub,
`core/` skeleton (`SseParser`, `AuthState`, `SecretStore`, `ApiConfig`), and the
App-Links login redirect wired in the manifest. **Login and chat follow in M3/M4.**

## App-Links — needed for login (M2/M3)

Login uses an https App-Link redirect to
`https://ai.datanet.ovh/app/login/callback`. For Android to open the app
automatically, the server must serve

```
https://ai.datanet.ovh/.well-known/assetlinks.json
```

containing this app's **signing-certificate SHA-256 fingerprint**. Get it from
your release keystore:

```sh
keytool -list -v -keystore <your.keystore> -alias <alias> | grep SHA256
```

Then hand that fingerprint over for the backend `assetlinks.json` (the manifest
intent-filter already sets `android:autoVerify="true"`).
