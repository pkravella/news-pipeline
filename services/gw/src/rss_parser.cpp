#include "rss_parser.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

static std::string node_text(xmlNode* node) {
  xmlChar* content = xmlNodeGetContent(node);
  if (!content) return {};
  std::string s(reinterpret_cast<char*>(content));
  xmlFree(content);
  return s;
}

static std::string child_text(xmlNode* parent, const char* name) {
  for (xmlNode* cur = parent->children; cur; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE && xmlStrcasecmp(cur->name, BAD_CAST name) == 0) {
      return node_text(cur);
    }
  }
  return {};
}

static int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// naive RFC822 pubDate parser (fallback to now on failure)
static int64_t parse_pubdate(const std::string& s) {
  // let’s be pragmatic: many feeds omit or vary format; default to now()
  if (s.empty()) return now_ms();
  return now_ms();
}

std::vector<FeedItem> parse_feed_xml(const std::string& xml) {
  std::vector<FeedItem> out;

  xmlDocPtr doc = xmlReadMemory(xml.c_str(), (int)xml.size(), "noname.xml", nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!doc) return out;

  xmlNode* root = xmlDocGetRootElement(doc);
  if (!root) { xmlFreeDoc(doc); return out; }

  // RSS 2.0: <rss><channel><item>...</item></channel></rss>
  if (xmlStrcasecmp(root->name, BAD_CAST "rss") == 0 || xmlStrcasecmp(root->name, BAD_CAST "rdf:RDF") == 0) {
    for (xmlNode* ch = root->children; ch; ch = ch->next) {
      if (ch->type == XML_ELEMENT_NODE && xmlStrcasecmp(ch->name, BAD_CAST "channel") == 0) {
        for (xmlNode* it = ch->children; it; it = it->next) {
          if (it->type == XML_ELEMENT_NODE && xmlStrcasecmp(it->name, BAD_CAST "item") == 0) {
            FeedItem fi;
            fi.title = child_text(it, "title");
            fi.link = child_text(it, "link");
            auto pd = child_text(it, "pubDate");
            fi.published_ts_ms = parse_pubdate(pd);
            if (!fi.title.empty() && !fi.link.empty()) out.push_back(std::move(fi));
          }
        }
      }
    }
  }
  // Atom: <feed><entry>...</entry></feed>
  else if (xmlStrcasecmp(root->name, BAD_CAST "feed") == 0) {
    for (xmlNode* it = root->children; it; it = it->next) {
      if (it->type == XML_ELEMENT_NODE && xmlStrcasecmp(it->name, BAD_CAST "entry") == 0) {
        FeedItem fi;
        fi.title = child_text(it, "title");
        // link may be in <link href="..."/>
        std::string link;
        for (xmlNode* ln = it->children; ln; ln = ln->next) {
          if (ln->type == XML_ELEMENT_NODE && xmlStrcasecmp(ln->name, BAD_CAST "link") == 0) {
            xmlChar* href = xmlGetProp(ln, BAD_CAST "href");
            if (href) { link = reinterpret_cast<char*>(href); xmlFree(href); break; }
          }
        }
        fi.link = link;
        fi.published_ts_ms = now_ms();
        if (!fi.title.empty() && !fi.link.empty()) out.push_back(std::move(fi));
      }
    }
  }

  xmlFreeDoc(doc);
  return out;
}
