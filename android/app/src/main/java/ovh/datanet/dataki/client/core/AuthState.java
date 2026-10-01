package ovh.datanet.dataki.client.core;

/**
 * Login state machine (docs/architecture.md §5.2). Captcha, 2FA, account locks
 * etc. never appear here — those are purely website states handled during the
 * browser hand-off. The client only ever sees success or abort/failure.
 */
public enum AuthState {
    /** No device key stored — show the "Sign in" button. */
    NEEDS_LOGIN,
    /** Browser opened, waiting for the App-Link redirect with code & state. */
    WAITING_FOR_BROWSER,
    /** Exchanging the one-time code for a device key. */
    EXCHANGING,
    /** Device key present and accepted. */
    AUTHENTICATED
}
