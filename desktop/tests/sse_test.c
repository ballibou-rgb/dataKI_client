/* GTK-free unit test for the SSE parser (chunk splitting, [DONE], CRLF). */

#include <glib.h>
#include <string.h>

#include "sse.h"

static void
collect(const char *payload, gpointer user_data)
{
  GPtrArray *out = user_data;
  g_ptr_array_add(out, g_strdup(payload));
}

static void
feed_str(DatakiSse *sse, const char *s)
{
  dataki_sse_feed(sse, s, strlen(s));
}

static void
test_simple(void)
{
  g_autoptr(GPtrArray) out = g_ptr_array_new_with_free_func(g_free);
  DatakiSse *sse = dataki_sse_new(collect, out);
  feed_str(sse, "data: {\"type\":\"chat_id\"}\n\n");
  feed_str(sse, "data: {\"type\":\"content_delta\",\"delta\":\"hi\"}\n\n");
  g_assert_cmpuint(out->len, ==, 2);
  g_assert_cmpstr(g_ptr_array_index(out, 0), ==, "{\"type\":\"chat_id\"}");
  dataki_sse_free(sse);
}

static void
test_split_and_done(void)
{
  g_autoptr(GPtrArray) out = g_ptr_array_new_with_free_func(g_free);
  DatakiSse *sse = dataki_sse_new(collect, out);
  feed_str(sse, "data: {\"type\":\"con");
  feed_str(sse, "tent_delta\",\"delta\":\"he");
  feed_str(sse, "llo\"}\n");
  feed_str(sse, "data: [DONE]\n\n");
  g_assert_cmpuint(out->len, ==, 1);
  g_assert_cmpstr(g_ptr_array_index(out, 0), ==,
                  "{\"type\":\"content_delta\",\"delta\":\"hello\"}");
  dataki_sse_free(sse);
}

static void
test_crlf_and_comment(void)
{
  g_autoptr(GPtrArray) out = g_ptr_array_new_with_free_func(g_free);
  DatakiSse *sse = dataki_sse_new(collect, out);
  feed_str(sse, ": keep-alive\r\n");
  feed_str(sse, "data: {\"x\":1}\r\n");
  feed_str(sse, "data: {\"y\":2}\r\n");
  g_assert_cmpuint(out->len, ==, 2);
  g_assert_cmpstr(g_ptr_array_index(out, 1), ==, "{\"y\":2}");
  dataki_sse_free(sse);
}

static void
test_flush_trailing(void)
{
  g_autoptr(GPtrArray) out = g_ptr_array_new_with_free_func(g_free);
  DatakiSse *sse = dataki_sse_new(collect, out);
  feed_str(sse, "data: {\"z\":3}"); /* no trailing newline */
  g_assert_cmpuint(out->len, ==, 0);
  dataki_sse_flush(sse);
  g_assert_cmpuint(out->len, ==, 1);
  g_assert_cmpstr(g_ptr_array_index(out, 0), ==, "{\"z\":3}");
  dataki_sse_free(sse);
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/sse/simple", test_simple);
  g_test_add_func("/sse/split-done", test_split_and_done);
  g_test_add_func("/sse/crlf-comment", test_crlf_and_comment);
  g_test_add_func("/sse/flush", test_flush_trailing);
  return g_test_run();
}
