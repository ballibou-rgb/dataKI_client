<?php

/*
 * dataKIWebUI — KI-Chat-Oberflaeche
 * Copyright (c) 2026 by dataNet.ovh Ltd. All rights reserved.
 * Origin: https://dataNet.ovh
 */
/**
 * api/v1/client/bootstrap.php
 *
 * Liefert alles, was der Client beim Start braucht: Profil, Betriebsmodus,
 * Token-Stand (nur Proxy-Modus), Modelle, Aufwandsstufen, Features,
 * Datei-Aktionen und Upload-Limits (Architektur §6.2 / §2.5).
 *
 * Auth: Bearer <Geräte-Key> (oder bestehende Session).
 *
 * @package Api
 */

require_once dirname(__DIR__, 3) . '/includes/auth.php';
require_once dirname(__DIR__, 3) . '/includes/proxy_client.php';
require_once dirname(__DIR__, 3) . '/includes/direct_client.php';

header('Content-Type: application/json; charset=utf-8');

$user = auth_current_user();
if (!$user) {
    json_error('Nicht angemeldet.', 401);
}

$models = get_chat_models($user);

// Token-/Tier-Stand nur im Proxy-Modus (alleinige Quelle: der Proxy).
$meData = (is_proxy_mode() && !empty($user['proxy_api_key']))
    ? proxy_me_get($user['proxy_api_key'])
    : null;
$tokenStatus = $meData['token_status']
    ?? ['tier' => '–', 'limit' => 0, 'tier_used' => 0, 'purchased' => 0, 'blocked' => false];

$ui = get_chat_ui_config();

json_response([
    'success'   => true,
    'user'      => [
        'id'    => (int)$user['id'],
        'name'  => (string)$user['name'],
        'email' => (string)$user['email'],
        'role'  => (string)$user['role'],
        'guest' => !empty($user['is_guest']),
    ],
    'chat_mode'     => CHAT_MODE,
    'token_status'  => $tokenStatus,
    'models'        => $models,
    'effort_levels' => get_effort_levels(),
    'effort_default'=> get_effort_default(),
    'features'      => [
        'websearch' => (bool)($ui['websearch']['on'] ?? false),
        'research'  => (bool)($ui['research']['on'] ?? false),
        'knowledge' => (int)get_setting('knowledge_enabled', '0') === 1,
        'tools'     => (int)get_setting('tools_enabled', '0') === 1,
        'image'     => (int)get_setting('image_enabled', '0') === 1,
        'voice'     => (int)get_setting('voice_enabled', '0') === 1,
        'code'      => (int)get_setting('code_enabled', '0') === 1,
    ],
    'file_actions'  => get_file_actions(),
    'plus_menu'     => get_plus_menu(),
    'limits'        => [
        'upload_max_bytes' => UPLOAD_MAX_BYTES,
        'allowed_ext'      => array_values(UPLOAD_ALLOWED_EXT),
    ],
]);
