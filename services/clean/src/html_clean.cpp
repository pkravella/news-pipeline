#include "html_clean.h"
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* body = reinterpret_cast<std::string*>(userdata);
  size_t total = size * nmemb;
  body->append(ptr, total);
  return total;
}

std::optional<std::string> fetch_html(const std::string& url, const HtmlFetchOptions& opt) {
  CURL* curl = curl_easy_init();
  if (!curl) return std::nullopt;

  std::string body;
  char errbuf[CURL_ERROR_SIZE]; errbuf[0] = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt.timeout_secs);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, opt.user_agent.c_str());
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    curl_easy_cleanup(curl);
    return std::nullopt;
  }
  long status = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);

  if (status < 200 || status >= 300) return std::nullopt;
  return body;
}

static std::string node_text(xmlNode* node) {
  xmlChar* content = xmlNodeGetContent(node);
  if (!content) return {};
  std::string s(reinterpret_cast<char*>(content));
  xmlFree(content);
  return s;
}

static void remove_nodes_by_name(xmlNode* root, const char* name) {
  for (xmlNode* cur = root; cur;) {
    xmlNode* next = cur->next;
    if (cur->type == XML_ELEMENT_NODE && xmlStrcasecmp(cur->name, BAD_CAST name) == 0) {
      xmlUnlinkNode(cur);
      xmlFreeNode(cur);
    } else if (cur->children) {
      remove_nodes_by_name(cur->children, name);
    }
    cur = next;
  }
}

static void remove_by_names(xmlNode* root, const std::vector<const char*>& names) {
  for (auto* n : names) remove_nodes_by_name(root, n);
}

static std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace((unsigned char)s[a])) ++a;
  while (b > a && std::isspace((unsigned char)s[b-1])) --b;
  return s.substr(a, b-a);
}

// Compute a crude "text density" score for a node
static size_t text_len(xmlNode* node) {
  size_t total = 0;
  for (xmlNode* cur = node; cur; cur = cur->next) {
    if (cur->type == XML_TEXT_NODE && cur->content) {
      total += std::strlen((char*)cur->content);
    }
    if (cur->children) total += text_len(cur->children);
  }
  return total;
}

static xmlNode* find_main_content(xmlNode* root) {
  // Prefer <article>, then the <div>/<section>/<main> with max text length,
  // skipping nav/header/footer/aside
  std::vector<xmlNode*> candidates;
  std::function<void(xmlNode*)> walk = [&](xmlNode* n) {
    if (!n) return;
    if (n->type == XML_ELEMENT_NODE) {
      if (!xmlStrcasecmp(n->name, BAD_CAST "article")) {
        candidates.push_back(n);
      } else if (!xmlStrcasecmp(n->name, BAD_CAST "div") ||
                 !xmlStrcasecmp(n->name, BAD_CAST "section") ||
                 !xmlStrcasecmp(n->name, BAD_CAST "main")) {
        // skip structural wrappers
        candidates.push_back(n);
      }
      for (xmlNode* c = n->children; c; c = c->next) walk(c);
    }
  };
  walk(root);

  // filter out nodes inside nav/header/footer/aside
  auto is_bad_ancestor = [](xmlNode* n) {
    for (xmlNode* p = n->parent; p; p = p->parent) {
      if (p->type == XML_ELEMENT_NODE &&
          (!xmlStrcasecmp(p->name, BAD_CAST "nav") ||
           !xmlStrcasecmp(p->name, BAD_CAST "header") ||
           !xmlStrcasecmp(p->name, BAD_CAST "footer") ||
           !xmlStrcasecmp(p->name, BAD_CAST "aside"))) return true;
    }
    return false;
  };

  xmlNode* best = nullptr; size_t best_score = 0;
  for (auto* n : candidates) {
    if (is_bad_ancestor(n)) continue;
    size_t score = text_len(n);
    if (score > best_score) { best_score = score; best = n; }
  }
  return best;
}

static std::string extract_title(xmlNode* root) {
  // try <meta property="og:title"> or <title> or first <h1>
  std::string title;
  // Scan <head> for title/meta
  std::function<void(xmlNode*)> scan_head = [&](xmlNode* n) {
    if (!n) return;
    if (n->type == XML_ELEMENT_NODE) {
      if (!xmlStrcasecmp(n->name, BAD_CAST "title")) {
        title = trim(node_text(n));
      }
      if (!xmlStrcasecmp(n->name, BAD_CAST "meta")) {
        xmlChar* prop = xmlGetProp(n, BAD_CAST "property");
        xmlChar* name = xmlGetProp(n, BAD_CAST "name");
        xmlChar* content = xmlGetProp(n, BAD_CAST "content");
        auto is_og = [](xmlChar* p, xmlChar* nm, const char* key) {
          if (p && !xmlStrcasecmp(p, BAD_CAST key)) return true;
          if (nm && !xmlStrcasecmp(nm, BAD_CAST key)) return true;
          return false;
        };
        if (content && (is_og(prop, name, "og:title") || is_og(prop, name, "twitter:title"))) {
          title = trim((char*)content);
        }
        if (prop) xmlFree(prop); if (name) xmlFree(name); if (content) xmlFree(content);
      }
    }
    for (xmlNode* c = n->children; c; c = c->next) scan_head(c);
  };
  // head node
  for (xmlNode* cur = root; cur; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE && !xmlStrcasecmp(cur->name, BAD_CAST "html")) {
      for (xmlNode* h = cur->children; h; h = h->next) {
        if (h->type == XML_ELEMENT_NODE && !xmlStrcasecmp(h->name, BAD_CAST "head")) {
          scan_head(h);
        }
      }
    }
  }
  if (!title.empty()) return title;

  // fallback: first h1 text
  std::function<xmlNode*(xmlNode*)> find_h1 = [&](xmlNode* n)->xmlNode* {
    if (!n) return nullptr;
    if (n->type == XML_ELEMENT_NODE && !xmlStrcasecmp(n->name, BAD_CAST "h1")) return n;
    for (xmlNode* c = n->children; c; c = c->next) {
      if (auto* got = find_h1(c)) return got;
    }
    return nullptr;
  };
  if (auto* h1 = find_h1(root)) return trim(node_text(h1));
  return "";
}

static std::string collect_text(xmlNode* n) {
  std::ostringstream oss;
  std::function<void(xmlNode*)> walk = [&](xmlNode* x) {
    if (!x) return;
    if (x->type == XML_TEXT_NODE && x->content) {
      oss << (char*)x->content;
    } else if (x->type == XML_ELEMENT_NODE) {
      // add newlines for block elements to avoid jammed text
      if (!xmlStrcasecmp(x->name, BAD_CAST "p") ||
          !xmlStrcasecmp(x->name, BAD_CAST "div") ||
          !xmlStrcasecmp(x->name, BAD_CAST "section") ||
          !xmlStrcasecmp(x->name, BAD_CAST "br") ||
          !xmlStrcasecmp(x->name, BAD_CAST "li")) {
        oss << "\n";
      }
    }
    for (xmlNode* c = x->children; c; c = c->next) walk(c);
  };
  walk(n);
  return trim(oss.str());
}

CleanResult clean_html_to_text(const std::string& url, const std::string& html) {
  CleanResult out;

  htmlDocPtr doc = htmlReadMemory(html.c_str(), (int)html.size(), "noname.html", nullptr,
                                  HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
  if (!doc) { return out; }

  xmlNode* root = xmlDocGetRootElement(doc);
  if (!root) { xmlFreeDoc(doc); return out; }

  // Drop scripts/styles/noscript
  remove_by_names(root, {"script","style","noscript","svg","iframe"});

  out.title = extract_title(root);

  // Find the biggest content node
  xmlNode* main = find_main_content(root);
  if (!main) main = root;

  out.body = collect_text(main);

  xmlFreeDoc(doc);

  out.language = detect_language_en_heuristic(out.body);
  out.hints = extract_hints(url, out.body);
  return out;
}

std::string detect_language_en_heuristic(const std::string& text) {
  if (text.empty()) return "unknown";
  size_t ascii_like = 0, total = 0;
  for (unsigned char c : text) {
    if (c == '\n' || c == '\r' || c == '\t' || std::isprint(c)) ascii_like++;
    total++;
    if (total > 5000) break; // sample the first ~5k chars
  }
  double ratio = (total == 0) ? 0.0 : (double)ascii_like / (double)total;
  return (ratio > 0.85) ? "en" : "unknown";
}

static std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}

static std::string host_from_url(const std::string& url) {
  // naive: find "://" then up to next "/" -> that's host
  auto p = url.find("://");
  if (p == std::string::npos) return "";
  auto rest = url.substr(p+3);
  auto slash = rest.find('/');
  std::string host = (slash == std::string::npos) ? rest : rest.substr(0, slash);
  return to_lower(host);
}

std::vector<std::string> extract_hints(const std::string& url, const std::string& text) {
  std::vector<std::string> hints;
  // cashtags
  std::regex cashtag(R"(?:^|[\s\(\[])[$]([A-Z]{1,5})(?:$|[\s\)\],\.!?;:]))");
  auto begin = std::sregex_iterator(text.begin(), text.end(), cashtag);
  auto end = std::sregex_iterator();
  std::set<std::string> uniq;
  for (auto it = begin; it != end; ++it) {
    auto match = *it;
    std::string sym = match.str(1);
    uniq.insert("$" + sym);
  }
  // host tokens
  std::string host = host_from_url(url);
  if (!host.empty()) uniq.insert("host:" + host);
  // path tokens
  auto p = url.find("://");
  std::string path = (p == std::string::npos) ? url : url.substr(p+3);
  auto slash = path.find('/');
  if (slash != std::string::npos) {
    std::string ponly = path.substr(slash+1);
    // split on /-_.
    std::string token;
    for (char c : ponly) {
      if (std::isalnum((unsigned char)c)) {
        token.push_back(std::tolower((unsigned char)c));
      } else {
        if (token.size() >= 3) uniq.insert("path:" + token);
        token.clear();
      }
    }
    if (token.size() >= 3) uniq.insert("path:" + token);
  }

  hints.assign(uniq.begin(), uniq.end());
  return hints;
}
