-- Initializes a table and a simple MV target for later (Phase 4 will push data here)

CREATE TABLE IF NOT EXISTS news_scored
(
  date Date DEFAULT toDate(ts),
  ts   DateTime64(3) DEFAULT now64(3),
  id   String,
  source String,
  url String,
  title String,
  tickers Array(String),
  sentiment Float32,
  topics Array(String)
)
ENGINE = MergeTree
PARTITION BY date
PRIMARY KEY (tickers, ts);
