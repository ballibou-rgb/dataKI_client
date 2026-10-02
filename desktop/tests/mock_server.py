#!/usr/bin/env python3
"""Mock dataKI backend for testing the native client's chat flow.

Serves just enough to exercise bootstrap + chat list + streaming without a real
LLM:  GET /api/v1/client/bootstrap.php, GET /chat_handler.php?action=list_chats,
and POST /chat_handler.php (send_message) → a chunked SSE stream.

Run:  python3 mock_server.py [port]
"""
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

BOOTSTRAP = {
    "success": True,
    "user": {"id": 1, "name": "Testuser", "email": "test@example.com", "role": "user", "guest": False},
    "chat_mode": "proxy",
    "token_status": {"tier": "free", "limit": 1000000, "tier_used": 12345, "purchased": 0, "blocked": False},
    "models": [
        {"id": "llama3", "label": "Llama 3", "model": "llama3", "tier": "free", "thinking": True},
        {"id": "qwen2.5", "label": "Qwen 2.5", "model": "qwen2.5", "tier": "free", "thinking": False},
    ],
    "effort_levels": [{"id": "mittel", "label": "Mittel", "temperature": 0.5}],
    "effort_default": "mittel",
    "features": {"websearch": True, "research": False, "knowledge": False,
                 "tools": False, "image": False, "voice": False, "code": False},
    "file_actions": [],
    "plus_menu": {},
    "limits": {"upload_max_bytes": 20971520, "allowed_ext": ["pdf", "png"]},
}

CHATS = {"success": True, "chats": [{"id": 1, "title": "Erste Unterhaltung"}]}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _json(self, obj):
        body = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path.endswith("/bootstrap.php"):
            return self._json(BOOTSTRAP)
        if u.path.endswith("/chat_handler.php") and parse_qs(u.query).get("action", [""])[0] == "list_chats":
            return self._json(CHATS)
        if u.path.endswith("/chat_handler.php") and parse_qs(u.query).get("action", [""])[0] == "get_messages":
            return self._json({"success": True, "messages": [
                {"role": "user", "content": "Frühere Frage"},
                {"role": "assistant", "content": "Frühere Antwort"}]})
        return self._json({"success": True})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        self.rfile.read(length)  # discard body
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        def send(obj):
            self.wfile.write(("data: " + json.dumps(obj) + "\n\n").encode())
            self.wfile.flush()

        send({"type": "chat_id", "chat_id": 42})
        send({"type": "thinking_delta", "delta": "Ich denke kurz nach… "})
        send({"type": "thinking_done"})
        for word in ["Hallo", " aus", " dem", " Mock", "-Server", "!", " Streaming", " funktioniert."]:
            send({"type": "content_delta", "delta": word})
            time.sleep(0.12)
        send({"type": "done", "message_id": 7,
              "token_status": {"tier": "free", "limit": 1000000, "tier_used": 12360, "purchased": 0, "blocked": False}})
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8098
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
