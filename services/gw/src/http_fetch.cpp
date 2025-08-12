#include "http_fetch.h"
#include <curl/curl.h>
#include <cstring>
#include <iostream>

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* body = reinterpret_cast<std::string*>(userdata);
  size_t total = size * nmemb;
  body->append(ptr, total);
  return total;
}

std::optional<HttpResponse> http_get(const std::string& url, const HttpOptions& opt) {
  CURL* curl = curl_easy_init();
  if (!curl) return std::nullopt;

  std::string body;
  char errbuf[CURL_ERROR_SIZE]; errbuf[0] = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, opt.user_agent.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt.timeout_secs);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  if (opt.accept_gzip) {
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  }

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "[http] curl error: " << (errbuf[0] ? errbuf : curl_easy_strerror(res)) << "\n";
    curl_easy_cleanup(curl);
    return std::nullopt;
  }

  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  char* eff = nullptr;
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff);
  std::string effective = eff ? std::string(eff) : url;

  curl_easy_cleanup(curl);
  return HttpResponse{status, std::move(body), std::move(effective)};
}
