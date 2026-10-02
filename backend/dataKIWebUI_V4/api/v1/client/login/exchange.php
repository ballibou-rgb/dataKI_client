<?php

/*
 * dataKIWebUI — KI-Chat-Oberflaeche
 * Copyright (c) 2026 by dataNet.ovh Ltd. All rights reserved.
 * Origin: https://dataNet.ovh
 */
/**
 * api/v1/client/login/exchange.php
 *
 * Tauscht den Einmal-Code aus dem Redirect gegen einen Geräte-Key
 * (Architektur §5.4 / §6.2). Methode: POST, Body JSON:
 *   { "code": "...", "device_name": "...", "client_version": "..." }
 *
 * Antwort:
 *   { "status":"ok", "device_key":"dk_…", "user":{...} }
 *   { "status":"invalid_or_expired", "error":"…" }
 *
 * @package Api
 */

require_once dirname(__DIR__, 4) . '/includes/auth.php';

header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    json_error('Nur POST-Anfragen werden unterstützt.', 405);
}

$raw  = file_get_contents('php://input');
$body = json_decode($raw ?: '', true);
if (!is_array($body)) {
    $body = $_POST;
}

$code       = trim((string)($body['code'] ?? ''));
$deviceName = trim((string)($body['device_name'] ?? ''));

if ($code === '') {
    json_response(['status' => 'invalid_or_expired', 'error' => 'Kein Code übergeben.'], 400);
}

$redeem = client_redeem_login_code($code);
if (!$redeem) {
    json_response(['status' => 'invalid_or_expired', 'error' => 'Code ungültig oder abgelaufen.'], 400);
}

$finalName = $redeem['device_name'] !== '' ? $redeem['device_name'] : ($deviceName !== '' ? $deviceName : 'Gerät');

$pdo  = Database::getConnection();
$stmt = $pdo->prepare('SELECT * FROM `users` WHERE `id` = :id AND `is_active` = 1 LIMIT 1');
$stmt->execute(['id' => $redeem['user_id']]);
$user = $stmt->fetch();
if (!$user) {
    json_response(['status' => 'invalid_or_expired', 'error' => 'Nutzer nicht gefunden oder gesperrt.'], 403);
}

$deviceKey = client_store_device_key((int)$user['id'], $finalName);

json_response([
    'status'     => 'ok',
    'device_key' => $deviceKey,
    'user'       => [
        'id'    => (int)$user['id'],
        'name'  => (string)$user['name'],
        'email' => (string)$user['email'],
        'role'  => (string)$user['role'],
    ],
]);
