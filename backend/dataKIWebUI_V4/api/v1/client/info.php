<?php

/*
 * dataKIWebUI — KI-Chat-Oberflaeche
 * Copyright (c) 2026 by dataNet.ovh Ltd. All rights reserved.
 * Origin: https://dataNet.ovh
 */
/**
 * api/v1/client/info.php
 *
 * Server-Handshake für den nativen Client (keine Auth). Liefert
 * App-/API-Version, Betriebsmodus und Upload-Limits.
 *
 * @package Api
 */

require_once dirname(__DIR__, 3) . '/includes/auth.php';

header('Content-Type: application/json; charset=utf-8');

json_response([
    'success'        => true,
    'app'            => 'dataKIWebUI',
    'app_version'    => APP_VERSION,
    'api_level'      => 1,
    'chat_mode'      => CHAT_MODE,
    'auth_mode'      => AUTH_MODE,
    'login_required' => auth_login_required(),
    'limits'         => [
        'upload_max_bytes' => UPLOAD_MAX_BYTES,
        'allowed_ext'      => array_values(UPLOAD_ALLOWED_EXT),
    ],
]);
