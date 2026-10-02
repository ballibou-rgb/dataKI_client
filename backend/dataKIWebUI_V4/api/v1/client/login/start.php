<?php

/*
 * dataKIWebUI — KI-Chat-Oberflaeche
 * Copyright (c) 2026 by dataNet.ovh Ltd. All rights reserved.
 * Origin: https://dataNet.ovh
 */
/**
 * api/v1/client/login/start.php
 *
 * Einstieg des Browser-Handoff-Logins (Architektur §5).
 *
 * Der native Client öffnet diese URL im System-Browser mit:
 *   ?redirect_uri=<Loopback oder App-Link>&state=<zufällig>&device_name=<Name>
 *
 * Ablauf:
 *  - Ist bereits eine Website-Session aktiv → Einmal-Code erzeugen und per
 *    302 an redirect_uri (?code=…&state=…) zurückleiten.
 *  - Sonst → Handoff-Parameter in der Session merken und zur normalen
 *    Website-Anmeldung (index.php) schicken. Nach erfolgreichem Login kehrt
 *    chat.php automatisch hierher zurück (client_consume_post_login_redirect).
 *
 * Es gibt KEINE native Passwort-/2FA-/Captcha-Eingabe — all das bleibt auf
 * der Website.
 *
 * @package Api
 */

require_once dirname(__DIR__, 4) . '/includes/auth.php';

auth_start_session();

$redirectUri = (string)($_GET['redirect_uri'] ?? '');
$state       = (string)($_GET['state'] ?? '');
$deviceName  = trim((string)($_GET['device_name'] ?? ''));
if ($deviceName === '') {
    $deviceName = 'Gerät';
}
$deviceName = mb_substr($deviceName, 0, 120);

if ($redirectUri === '' || !client_validate_redirect_uri($redirectUri)) {
    http_response_code(400);
    header('Content-Type: text/plain; charset=utf-8');
    exit('Ungültige redirect_uri.');
}

// Handoff-Parameter für die Dauer der Session merken.
$_SESSION['client_login'] = [
    'redirect_uri' => $redirectUri,
    'state'        => $state,
    'device_name'  => $deviceName,
    'ts'           => time(),
];

/**
 * Schließt den Handoff ab: Einmal-Code erzeugen und an redirect_uri leiten.
 */
function client_finish_login_handoff(array $user): void
{
    $cl = $_SESSION['client_login'] ?? null;
    if (!$cl) {
        http_response_code(400);
        exit('Kein Login-Vorgang aktiv.');
    }
    unset($_SESSION['client_login']);

    $code = client_issue_login_code((int)$user['id'], (string)$cl['device_name']);
    $sep  = (strpos((string)$cl['redirect_uri'], '?') !== false) ? '&' : '?';
    $loc  = $cl['redirect_uri'] . $sep
          . 'code=' . rawurlencode($code)
          . '&state=' . rawurlencode((string)$cl['state']);

    header('Location: ' . $loc);
    exit;
}

$user = auth_current_user();
if ($user && empty($user['is_guest'])) {
    client_finish_login_handoff($user);
    // kehrt nicht zurück
}

// Nicht angemeldet: zur Website-Anmeldung, danach hierher zurück.
$self = url('/api/v1/client/login/start.php')
      . '?redirect_uri=' . rawurlencode($redirectUri)
      . '&state=' . rawurlencode($state)
      . '&device_name=' . rawurlencode($deviceName);
$_SESSION['client_post_login_redirect'] = $self;

header('Location: ' . url('/index.php'));
exit;
