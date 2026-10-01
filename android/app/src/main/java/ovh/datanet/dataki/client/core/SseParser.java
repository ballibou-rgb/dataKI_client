package ovh.datanet.dataki.client.core;

import java.util.ArrayList;
import java.util.List;

/**
 * Incremental Server-Sent-Events parser (docs/architecture.md §9.4).
 *
 * The chat stream delivers one JSON object per {@code data:} line, but network
 * chunks split lines arbitrarily — including in the middle of a multi-byte
 * UTF-8 character. This parser buffers incoming text and only emits complete
 * lines (up to the last newline), keeping the remainder for the next feed.
 *
 * It is deliberately UI- and network-free so it can be unit tested in isolation
 * (see src/test/.../SseParserTest.java). Payloads are returned as the raw JSON
 * string after {@code data:}; {@code [DONE]} and blank lines are ignored.
 */
public final class SseParser {

    /** Receives the JSON payload of each complete {@code data:} event line. */
    public interface Listener {
        void onData(String json);
    }

    private final StringBuilder buffer = new StringBuilder();
    private final Listener listener;

    public SseParser(Listener listener) {
        this.listener = listener;
    }

    /** Feed a decoded text chunk. Safe to call with partial lines. */
    public void feed(String chunk) {
        if (chunk == null || chunk.isEmpty()) {
            return;
        }
        buffer.append(chunk);

        int newline;
        while ((newline = indexOfLineBreak(buffer)) >= 0) {
            String line = buffer.substring(0, newline);
            // Drop the line break (handle \r\n as well as \n).
            int advance = newline + 1;
            if (newline < buffer.length() && buffer.charAt(newline) == '\r'
                    && advance < buffer.length() && buffer.charAt(advance) == '\n') {
                advance++;
            }
            buffer.delete(0, advance);
            handleLine(line);
        }
    }

    /** Convenience: parse a whole response body already in memory. */
    public static List<String> parseAll(String body) {
        final List<String> out = new ArrayList<>();
        SseParser p = new SseParser(out::add);
        p.feed(body);
        // A trailing line without a final newline is still a valid event.
        p.flush();
        return out;
    }

    /** Process any buffered trailing line that had no final newline. */
    public void flush() {
        if (buffer.length() > 0) {
            String line = buffer.toString();
            buffer.setLength(0);
            handleLine(line);
        }
    }

    private void handleLine(String rawLine) {
        String line = stripTrailingCr(rawLine).trim();
        if (line.isEmpty() || line.startsWith(":")) {
            return; // blank line or SSE comment
        }
        if (!line.startsWith("data:")) {
            return; // ignore non-data fields (event:, id:, retry:)
        }
        String payload = line.substring("data:".length()).trim();
        if (payload.isEmpty() || "[DONE]".equals(payload)) {
            return;
        }
        listener.onData(payload);
    }

    private static int indexOfLineBreak(CharSequence s) {
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '\n' || c == '\r') {
                return i;
            }
        }
        return -1;
    }

    private static String stripTrailingCr(String s) {
        return s.endsWith("\r") ? s.substring(0, s.length() - 1) : s;
    }
}
