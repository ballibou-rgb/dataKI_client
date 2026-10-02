#include "http.h"

#include <string.h>
#include <curl/curl.h>

#include "dataki-config.h"

#define DATAKI_HTTP_ERROR (dataki_http_error_quark())

static GQuark
dataki_http_error_quark(void)
{
  return g_quark_from_static_string("dataki-http-error");
}

void
dataki_http_response_clear(DatakiHttpResponse *resp)
{
  if (resp == NULL)
    return;
  g_clear_pointer(&resp->body, g_free);
  resp->length = 0;
  resp->status = 0;
}

static size_t
write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  GString *buf = userdata;
  gsize total = size * nmemb;
  g_string_append_len(buf, ptr, total);
  return total;
}

static struct curl_slist *
build_headers(const char *bearer, gboolean json_body)
{
  struct curl_slist *list = NULL;
  list = curl_slist_append(list, "X-Client: dataKI-native/" DATAKI_VERSION);
  list = curl_slist_append(list, "X-Client-Api: 1");
  list = curl_slist_append(list, "Accept: application/json");

  if (json_body)
    list = curl_slist_append(list, "Content-Type: application/json");

  if (bearer != NULL && *bearer != '\0')
    {
      g_autofree char *auth = g_strdup_printf("Authorization: Bearer %s", bearer);
      list = curl_slist_append(list, auth);
    }
  return list;
}

static gboolean
perform(const char *url, const char *bearer, const char *post_body,
        DatakiHttpResponse *out, GError **error)
{
  g_return_val_if_fail(url != NULL, FALSE);
  g_return_val_if_fail(out != NULL, FALSE);

  CURL *curl = curl_easy_init();
  if (curl == NULL)
    {
      g_set_error_literal(error, DATAKI_HTTP_ERROR, 0, "curl init failed");
      return FALSE;
    }

  GString *buf = g_string_new(NULL);
  struct curl_slist *headers = build_headers(bearer, post_body != NULL);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "dataKI-native/" DATAKI_VERSION);

  if (post_body != NULL)
    {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post_body));
    }

  CURLcode rc = curl_easy_perform(curl);

  gboolean ok = FALSE;
  if (rc != CURLE_OK)
    {
      g_set_error(error, DATAKI_HTTP_ERROR, 1, "%s", curl_easy_strerror(rc));
    }
  else
    {
      long status = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
      out->status = status;
      out->length = buf->len;
      out->body   = g_string_free(buf, FALSE);
      buf = NULL;
      ok = TRUE;
    }

  if (buf != NULL)
    g_string_free(buf, TRUE);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return ok;
}

gboolean
dataki_http_get(const char *url, const char *bearer,
                DatakiHttpResponse *out, GError **error)
{
  return perform(url, bearer, NULL, out, error);
}

gboolean
dataki_http_post_json(const char *url, const char *bearer,
                      const char *json_body,
                      DatakiHttpResponse *out, GError **error)
{
  return perform(url, bearer, json_body ? json_body : "", out, error);
}
