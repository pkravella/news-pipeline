#include "url_norm.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// very lightweight URL normalization (good enough to start)
// - lowercases scheme/host
// - strips fragment
// - removes common utm_* query params
// - trims trailing '/' (except if path is just '/')
static std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}

static bool starts_with(const std::string& s, const std::string& p) {
  return s.rfind(p, 0) == 0;
}

std::string normalize_url(const std::string& in) {
  if (in.empty()) return in;
  std::string url = in;

  // strip fragment
  if (auto pos = url.find('#'); pos != std::string::npos) {
    url.erase(pos);
  }

  // split on "://"
  auto scheme_pos = url.find("://");
  if (scheme_pos == std::string::npos) return url;
  std::string scheme = to_lower(url.substr(0, scheme_pos));
  std::string rest = url.substr(scheme_pos + 3);

  // host[:port]/path?query
  auto slash = rest.find('/');
  std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);
  std::string pathquery = (slash == std::string::npos) ? "" : rest.substr(slash);

  // lowercase host
  hostport = to_lower(hostport);

  // remove common www.
  if (starts_with(hostport, "www.")) hostport = hostport.substr(4);

  // separate path and query
  std::string path = pathquery;
  std::string query;
  if (auto qpos = pathquery.find('?'); qpos != std::string::npos) {
    path = pathquery.substr(0, qpos);
    query = pathquery.substr(qpos + 1);
  }

  // remove trailing slash (but keep "/" root)
  if (path.size() > 1 && path.back() == '/') path.pop_back();

  // drop utm_* from query
  if (!query.empty()) {
    std::string outq;
    size_t start = 0;
    while (start < query.size()) {
      auto amp = query.find('&', start);
      auto part = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
      if (!(starts_with(part, "utm_") || starts_with(part, "fbclid="))) {
        if (!outq.empty()) outq.push_back('&');
        outq += part;
      }
      if (amp == std::string::npos) break;
      start = amp + 1;
    }
    query = outq;
  }

  std::string out = scheme + "://" + hostport + path;
  if (!query.empty()) out += "?" + query;
  return out;
}
