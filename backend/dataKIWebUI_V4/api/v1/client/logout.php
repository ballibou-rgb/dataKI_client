<?php

/*
 * dataKIWebUI — KI-Chat-Oberflaeche
 * Copyright (c) 2026 by dataNet.ovh Ltd. All rights reserved.
 * Origin: https://dataNet.ovh
 */
/**
 * api/v1/client/logout.php
 *
 * Widerruft den Geräte-Key, mit dem die Anfrage authentifiziert ist.
 * Auth: Bearer <Geräte-Key>. Methode: POST.
 *
 * @package Api
 */

require_once dirname(__DIR__, 3) . '/includes/auth.php';

header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    json_error('Nur POST-Anfragen werden unterstützt.', 405);
}

$token   = client_bearer_token();
$revoked = client_revoke_device_key($token);

json_response(['success' => true, 'revoked' => $revoked]);
