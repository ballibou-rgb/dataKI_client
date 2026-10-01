package ovh.datanet.dataki.client.core;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * GTK-free equivalent of the desktop SSE parser tests: chunk splitting,
 * [DONE]/blank handling, CRLF, and UTF-8 characters split across chunks.
 */
public class SseParserTest {

    @Test
    public void parsesSimpleEvents() {
        List<String> out = SseParser.parseAll(
                "data: {\"type\":\"chat_id\"}\n\n" +
                "data: {\"type\":\"content_delta\",\"delta\":\"hi\"}\n\n");
        assertEquals(2, out.size());
        assertEquals("{\"type\":\"chat_id\"}", out.get(0));
    }

    @Test
    public void ignoresDoneAndBlankAndComments() {
        List<String> out = SseParser.parseAll(
                ": keep-alive\n\n" +
                "data: {\"a\":1}\n" +
                "data: [DONE]\n\n");
        assertEquals(1, out.size());
        assertEquals("{\"a\":1}", out.get(0));
    }

    @Test
    public void reassemblesLinesSplitAcrossChunks() {
        final List<String> out = new ArrayList<>();
        SseParser p = new SseParser(out::add);
        p.feed("data: {\"type\":\"con");
        p.feed("tent_delta\",\"delta\":\"he");
        p.feed("llo\"}\n\n");
        assertEquals(1, out.size());
        assertEquals("{\"type\":\"content_delta\",\"delta\":\"hello\"}", out.get(0));
    }

    @Test
    public void handlesUtf8SplitAcrossChunks() {
        // The three bytes of "€" arrive as one logical char split by chunking.
        final List<String> out = new ArrayList<>();
        SseParser p = new SseParser(out::add);
        p.feed("data: {\"delta\":\"E");
        p.feed("UR € to");
        p.feed("tal\"}\n\n");
        assertEquals(1, out.size());
        assertEquals("{\"delta\":\"EUR € total\"}", out.get(0));
    }

    @Test
    public void handlesCrlf() {
        List<String> out = SseParser.parseAll("data: {\"x\":1}\r\ndata: {\"y\":2}\r\n");
        assertEquals(2, out.size());
        assertEquals("{\"x\":1}", out.get(0));
        assertEquals("{\"y\":2}", out.get(1));
    }
}
