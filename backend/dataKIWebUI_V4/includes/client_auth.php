<?php

/*
 * dataKIWebUI — KI-Chat-Oberflaeche
 * Copyright (c) 2026 by dataNet.ovh Ltd. All rights reserved.
 * Origin: https://dataNet.ovh
 *
 * Dieser Urheber- und Herkunftsvermerk ist Bestandteil der Software und
 * darf gemaess Lizenz nicht entfernt werden.
 */
/**
 * includes/client_auth.php
 *
 * Native-Client-Authentifizierung (dataKI Desktop/Android).
 *
 * Erweitert die bestehende Auth um einen geräte-gebundenen Bearer-Key
 * ("Geräte-Key", Format dk_...), mit dem der native Client dieselben
 * Handler nutzt wie die WebUI (Option A der Architektur, Abschnitt 3/5/6).
 *
 * Der Geräte-Key wird serverseitig NUR als SHA-256-Hash gespeichert und ist
 * in `server_keys` mit kind='device' abgelegt. Er darf alles, was eine
 * normale Chat-Session darf, aber KEINE Admin-/Konto-Sicherheitsaktionen
 * (siehe auth_require_admin()).
 *
 * @package Includes
 */

require_once __DIR__ . '/db.php';
require_once __DIR__ . '/functions.php';

/** TTL des Einmal-Codes aus login/start (Sekunden). */
const CLIENT_LOGIN_CODE_TTL = 120;

/**
 * Liest den Bearer-Token robust aus den üblichen Quellen (Apache/FPM/CGI
 * entfernen den Authorization-Header oft aus $_SERVER). Identisch zur
 * Logik in api/v1/completions.php, hier wiederverwendbar.
 *
 * @return string Token ohne "Bearer "-Präfix, oder '' wenn keiner da ist.
 */
function client_bearer_token(): string
{
    $h = $_SERVER['HTTP_AUTHORIZATION'] ?? '';
    if ($h === '' && !empty($_SERVER['REDIRECT_HTTP_AUTHORIZATION'])) {
        $h = $_SERVER['REDIRECT_HTTP_AUTHORIZATION'];
    }
    if ($h === '' && function_exists('apache_request_headers')) {
        foreach (apache_request_headers() as $k => $v) {
            if (strcasecmp($k, 'Authorization') === 0) { $h = $v; break; }
        }
    }
    if ($h === '' && function_exists('getallheaders')) {
        foreach (getallheaders() as $k => $v) {
            if (strcasecmp($k, 'Authorization') === 0) { $h = $v; break; }
        }
    }
    if (preg_match('/^Bearer\s+(.+)$/i', trim((string)$h), $m)) {
        return trim($m[1]);
    }
    return '';
}

/**
 * Prüft, ob eine Spalte in einer Tabelle existiert.
 */
function client_column_exists(PDO $pdo, string $table, string $column): bool
{
    // SHOW COLUMNS … LIKE ? erlaubt keine gebundenen Parameter. Die Werte
    // stammen ausschließlich aus eigenem Code (keine Nutzereingabe); dennoch
    // auf [A-Za-z0-9_] beschränken und sauber quoten.
    $t = preg_replace('/[^A-Za-z0-9_]/', '', $table);
    $c = str_replace(['\\', "'"], ['\\\\', "\\'"], $column);
    $stmt = $pdo->query("SHOW COLUMNS FROM `{$t}` LIKE '{$c}'");
    return $stmt !== false && (bool)$stmt->fetch();
}

/**
 * Prüft, ob ein Index auf einer Tabelle existiert.
 */
function client_index_exists(PDO $pdo, string $table, string $index): bool
{
    $t = preg_replace('/[^A-Za-z0-9_]/', '', $table);
    $k = str_replace(['\\', "'"], ['\\\\', "\\'"], $index);
    $stmt = $pdo->query("SHOW INDEX FROM `{$t}` WHERE Key_name = '{$k}'");
    return $stmt !== false && (bool)$stmt->fetch();
}

/**
 * Idempotente Auto-Migration (nach dem Muster der bestehenden
 * SHOW COLUMNS / ALTER TABLE-Migrationen). Erweitert `server_keys` um die
 * Geräte-Key-Felder und legt die Einmal-Code-Tabelle an.
 */
function client_migrate(): void
{
    static $done = false;
    if ($done) {
        return;
    }

    $pdo = Database::getConnection();

    // Schon migriert? (key_hash als Marker)
    if (client_column_exists($pdo, 'server_keys', 'key_hash')) {
        $done = true;
        return;
    }

    // api_key darf für Geräte-Keys NULL sein (sie nutzen nur key_hash).
    $pdo->exec("ALTER TABLE `server_keys` MODIFY `api_key` VARCHAR(128) NULL");

    if (!client_column_exists($pdo, 'server_keys', 'kind')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD COLUMN `kind` VARCHAR(16) NOT NULL DEFAULT 'gateway' COMMENT 'gateway|device'");
    }
    if (!client_column_exists($pdo, 'server_keys', 'device_name')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD COLUMN `device_name` VARCHAR(191) NULL");
    }
    if (!client_column_exists($pdo, 'server_keys', 'key_hash')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD COLUMN `key_hash` CHAR(64) NULL COMMENT 'SHA-256 des Geräte-Keys'");
    }
    if (!client_column_exists($pdo, 'server_keys', 'expires_at')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD COLUMN `expires_at` TIMESTAMP NULL");
    }
    if (!client_column_exists($pdo, 'server_keys', 'revoked_at')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD COLUMN `revoked_at` TIMESTAMP NULL");
    }
    if (!client_column_exists($pdo, 'server_keys', 'last_ip')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD COLUMN `last_ip` VARCHAR(45) NULL");
    }
    if (!client_index_exists($pdo, 'server_keys', 'uq_server_keys_key_hash')) {
        $pdo->exec("ALTER TABLE `server_keys` ADD UNIQUE KEY `uq_server_keys_key_hash` (`key_hash`)");
    }

    $pdo->exec(
        "CREATE TABLE IF NOT EXISTS `client_login_codes` (
            `id`          INT UNSIGNED NOT NULL AUTO_INCREMENT,
            `code_hash`   CHAR(64) NOT NULL,
            `user_id`     INT UNSIGNED NOT NULL,
            `device_name` VARCHAR(191) NULL,
            `expires_at`  TIMESTAMP NOT NULL,
            `used_at`     TIMESTAMP NULL,
            `created_at`  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (`id`),
            UNIQUE KEY `uq_client_login_code` (`code_hash`),
            KEY `idx_client_login_user` (`user_id`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Einmal-Codes für den Native-Client-Login-Handoff'"
    );

    $done = true;
}

/**
 * Löst einen Geräte-Key (Bearer dk_...) auf den zugehörigen Nutzer auf.
 * Aktualisiert last_used_at/last_ip. Setzt $GLOBALS['auth_via_device_key'].
 *
 * @return array<string,mixed>|null Nutzer bei gültigem Key, sonst null.
 */
function client_device_key_user(): ?array
{
    $token = client_bearer_token();
    if ($token === '' || strncmp($token, 'dk_', 3) !== 0) {
        return null;
    }

    client_migrate();
    $pdo  = Database::getConnection();
    $hash = hash('sha256', $token);

    $stmt = $pdo->prepare(
        "SELECT * FROM `server_keys`
         WHERE `key_hash` = :h AND `kind` = 'device'
           AND `revoked_at` IS NULL
           AND (`expires_at` IS NULL OR `expires_at` > NOW())
         LIMIT 1"
    );
    $stmt->execute(['h' => $hash]);
    $keyRow = $stmt->fetch();
    if (!$keyRow) {
        return null;
    }

    $pdo->prepare('UPDATE `server_keys` SET `last_used_at` = NOW(), `last_ip` = :ip WHERE `id` = :id')
        ->execute(['ip' => $_SERVER['REMOTE_ADDR'] ?? null, 'id' => $keyRow['id']]);

    $stmt = $pdo->prepare('SELECT * FROM `users` WHERE `id` = :id AND `is_active` = 1 LIMIT 1');
    $stmt->execute(['id' => $keyRow['user_id']]);
    $user = $stmt->fetch();
    if (!$user) {
        return null;
    }

    $GLOBALS['auth_via_device_key'] = true;
    return $user;
}

/**
 * Erzeugt einen neuen Geräte-Key (nur einmal im Klartext verfügbar).
 *
 * @return string dk_ + 40 Hex-Zeichen
 */
function client_generate_device_key(): string
{
    return 'dk_' . bin2hex(random_bytes(20));
}

/**
 * Legt einen Geräte-Key für einen Nutzer an (nur der Hash wird gespeichert).
 *
 * @return string Der Klartext-Key (einmalig zurückgeben, dann verwerfen).
 */
function client_store_device_key(int $userId, string $deviceName): string
{
    client_migrate();
    $pdo = Database::getConnection();
    $key = client_generate_device_key();

    $stmt = $pdo->prepare(
        "INSERT INTO `server_keys` (`user_id`, `api_key`, `kind`, `device_name`, `key_hash`, `last_ip`, `label`)
         VALUES (:uid, NULL, 'device', :dname, :hash, :ip, :label)"
    );
    $stmt->execute([
        'uid'   => $userId,
        'dname' => $deviceName !== '' ? $deviceName : 'Gerät',
        'hash'  => hash('sha256', $key),
        'ip'    => $_SERVER['REMOTE_ADDR'] ?? null,
        'label' => $deviceName !== '' ? $deviceName : 'Gerät',
    ]);

    return $key;
}

/**
 * Widerruft den Geräte-Key, mit dem die aktuelle Anfrage authentifiziert ist.
 */
function client_revoke_device_key(string $token): bool
{
    if ($token === '' || strncmp($token, 'dk_', 3) !== 0) {
        return false;
    }
    client_migrate();
    $pdo = Database::getConnection();
    $stmt = $pdo->prepare(
        "UPDATE `server_keys` SET `revoked_at` = NOW()
         WHERE `key_hash` = :h AND `kind` = 'device' AND `revoked_at` IS NULL"
    );
    $stmt->execute(['h' => hash('sha256', $token)]);
    return $stmt->rowCount() > 0;
}

/**
 * Erzeugt einen Einmal-Code (login/start → Redirect), gebunden an Nutzer +
 * Gerätename, TTL CLIENT_LOGIN_CODE_TTL. Nur der Hash wird gespeichert.
 *
 * @return string Klartext-Code
 */
function client_issue_login_code(int $userId, string $deviceName): string
{
    client_migrate();
    $pdo  = Database::getConnection();
    $code = bin2hex(random_bytes(24));

    $stmt = $pdo->prepare(
        "INSERT INTO `client_login_codes` (`code_hash`, `user_id`, `device_name`, `expires_at`)
         VALUES (:h, :uid, :dname, DATE_ADD(NOW(), INTERVAL :ttl SECOND))"
    );
    $stmt->execute([
        'h'     => hash('sha256', $code),
        'uid'   => $userId,
        'dname' => $deviceName,
        'ttl'   => CLIENT_LOGIN_CODE_TTL,
    ]);

    return $code;
}

/**
 * Löst einen Einmal-Code ein (login/exchange). Einmalig gültig.
 *
 * @return array{user_id:int,device_name:string}|null
 */
function client_redeem_login_code(string $code): ?array
{
    if ($code === '') {
        return null;
    }
    client_migrate();
    $pdo  = Database::getConnection();
    $hash = hash('sha256', $code);

    $stmt = $pdo->prepare(
        "SELECT * FROM `client_login_codes`
         WHERE `code_hash` = :h AND `used_at` IS NULL AND `expires_at` > NOW()
         LIMIT 1"
    );
    $stmt->execute(['h' => $hash]);
    $row = $stmt->fetch();
    if (!$row) {
        return null;
    }

    // Sofort als benutzt markieren (einmalig).
    $upd = $pdo->prepare('UPDATE `client_login_codes` SET `used_at` = NOW() WHERE `id` = :id AND `used_at` IS NULL');
    $upd->execute(['id' => $row['id']]);
    if ($upd->rowCount() === 0) {
        return null; // Race: schon eingelöst
    }

    return [
        'user_id'     => (int)$row['user_id'],
        'device_name' => (string)($row['device_name'] ?? ''),
    ];
}

/**
 * Validiert eine redirect_uri für den Login-Handoff (Open-Redirect-Schutz).
 *
 * Erlaubt:
 *  - Loopback (Desktop):   http://127.0.0.1:<port>/...  und  http://localhost:<port>/...
 *  - App-Link (Android):   https://<eigener Host>/app/login/callback...
 *
 * @return bool
 */
function client_validate_redirect_uri(string $uri): bool
{
    $p = parse_url($uri);
    if (!$p || empty($p['scheme']) || empty($p['host'])) {
        return false;
    }

    $scheme = strtolower($p['scheme']);
    $host   = strtolower($p['host']);

    // Desktop-Loopback
    if ($scheme === 'http' && ($host === '127.0.0.1' || $host === 'localhost' || $host === '[::1]')) {
        return true;
    }

    // Android-App-Link: gleicher Host wie die WebUI, fester Pfad.
    if ($scheme === 'https' && $host === strtolower($_SERVER['HTTP_HOST'] ?? '')) {
        $path = $p['path'] ?? '';
        if (strpos($path, '/app/login/callback') === 0 || strpos($path, '/api/v1/client/') === 0) {
            return true;
        }
    }

    return false;
}

/**
 * Liefert (und löscht) ein serverseitig gemerktes Post-Login-Ziel.
 * Wird von login/start gesetzt, damit die bestehende Website-Anmeldung
 * danach zum Client-Handoff zurückkehrt. Der Wert stammt ausschließlich
 * aus der eigenen Session (nicht aus Nutzereingaben).
 *
 * @return string|null
 */
function client_consume_post_login_redirect(): ?string
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        return null;
    }
    $target = $_SESSION['client_post_login_redirect'] ?? null;
    unset($_SESSION['client_post_login_redirect']);
    return is_string($target) && $target !== '' ? $target : null;
}
