#include "yahoo_html.h"
#include "http_fetch.h"
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>

static std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}

long long now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string yahoo_html_url_for(const std::string& ticker, const YahooHtmlConfig& cfg) {
  std::string out = cfg.url_template;
  const std::string from = "{TICKER}";
  size_t pos = 0;
  while ((pos = out.find(from, pos)) != std::string::npos) {
    out.replace(pos, from.size(), ticker);
    pos += ticker.size();
  }
  return out;
}

// Naive robots.txt check: fetch once, ensure no "Disallow: /quote/" or "/quote/*/news" rules.
// This is a coarse guard — not a full robots parser. If disallowed, we return false.
bool yahoo_html_robots_allows(const YahooHtmlConfig& cfg) {
  static bool cached = false;
  static bool allowed_cache = true;

  if (cached) return allowed_cache;

  HttpOptions opt;
  opt.user_agent = cfg.user_agent;
  opt.timeout_secs = cfg.http_timeout_secs;
  auto resp = http_get(cfg.robots_url, opt);
  if (!resp || resp->status != 200) {
    std::cerr << "[yahoo_html] robots.txt fetch failed or non-200. Defaulting to conservative allow=true.\n";
    cached = true;
    allowed_cache = true;
    return allowed_cache;
  }

  std::string body = to_lower(resp->body);
  // very simple: if we find 'disallow: /quote/' we consider it disallowed.
  // (Adjust if you want stricter matching.)
  if (body.find("disallow: /quote/") != std::string::npos) {
    std::cerr << "[yahoo_html] robots.txt indicates /quote/ is disallowed. Skipping HTML adapter.\n";
    cached = true;
    allowed_cache = false;
    return allowed_cache;
  }

  cached = true;
  allowed_cache = true;
  return allowed_cache;
}

// Helper: read attribute value of a node
static std::string attr(xmlNode* node, const char* name) {
  xmlChar* v = xmlGetProp(node, BAD_CAST name);
  if (!v) return {};
  std::string s = reinterpret_cast<char*>(v);
  xmlFree(v);
  return s;
}

// Helper: get visible text content of an element (trimmed)
static void collect_text(xmlNode* node, std::string& out) {
  for (xmlNode* cur = node; cur; cur = cur->next) {
    if (cur->type == XML_TEXT_NODE && cur->content) {
      out.append(reinterpret_cast<char*>(cur->content));
    }
    if (cur->children) collect_text(cur->children, out);
  }
}

static std::string trim(std::string s) {
  auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c){return std::isspace(c);});
  auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c){return std::isspace(c);}).base();
  return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

// Extract links that look like Yahoo Finance news items.
// Heuristic: <a href="https://finance.yahoo.com/...."> and URL contains "/news/"
// Title = anchor text (trimmed). Timestamp: not always present, default to now.
std::vector<YahooHtmlItem> yahoo_html_extract_items(const std::string& html, int max_links) {
  std::vector<YahooHtmlItem> out;
  if (html.empty()) return out;

  htmlDocPtr doc = htmlReadMemory(html.c_str(), (int)html.size(), "noname.html", nullptr,
                                  HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
  if (!doc) return out;

  std::unordered_set<std::string> seen; // avoid dup links within the page

  xmlNode* root = xmlDocGetRootElement(doc);
  std::vector<xmlNode*> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    xmlNode* node = stack.back();
    stack.pop_back();

    if (node->type == XML_ELEMENT_NODE && xmlStrcasecmp(node->name, BAD_CAST "a") == 0) {
      std::string href = attr(node, "href");
      if (!href.empty()) {
        // Make absolute if protocol-relative e.g., //finance.yahoo.com/...
        if (href.rfind("//finance.yahoo.com", 0) == 0) href = "https:" + href;

        // Filter: must be Yahoo domain and contain "/news/"
        std::string low = to_lower(href);
        if (low.find("finance.yahoo.com") != std::string::npos &&
            low.find("/news/") != std::string::npos &&
            seen.insert(href).second) {

          std::string text;
          collect_text(node->children, text);
          text = trim(text);

          if (!text.empty()) {
            YahooHtmlItem it;
            it.title = text;
            it.url = href;
            it.published_ts_ms = now_ms(); // we don’t have the exact article time here
            out.push_back(std::move(it));
            if ((int)out.size() >= max_links) break;
          }
        }
      }
    }

    for (xmlNode* c = node->children; c; c = c->next) stack.push_back(c);
  }

  xmlFreeDoc(doc);
  return out;
}
