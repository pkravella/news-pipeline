#include <fmt/core.h>
#include "news.pb.h"

int main() {
  fmt::print("[news_clean] Cleaner stub online. (Cleaner logic arrives in Phase 2)\n");
  finnews::ArticleClean c;
  c.set_id("demo");
  c.set_title("demo-title");
  return 0;
}
