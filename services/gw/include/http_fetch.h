#pragma once
#include <string>
#include <optional>
#include <vector>

struct HttpResponse {
  long status = 0;
  std::string body;
  std::string effective_url;
};

struct HttpOptions {
  std::string user_agent = "FinNewsBot/1.0";
  long timeout_secs = 10;
  bool accept_gzip = true;
};

std::optional<HttpResponse> http_get(const std::string& url, const HttpOptions& opt);
