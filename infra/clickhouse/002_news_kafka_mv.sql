-- (Re)create Kafka Engine table and MV for news.scored -> news_scored

-- Clean up old objects if they exist
DROP VIEW IF EXISTS mv_news_scored;
DROP TABLE IF EXISTS news_scored_kafka;

-- This table consumes from Kafka topic 'news.scored'.
-- We ingest message VALUE as a raw JSON string (column: payload).
CREATE TABLE news_scored_kafka
(
  payload String
)
ENGINE = Kafka
SETTINGS
  kafka_broker_list = 'kafka:9092',
  kafka_topic_list = 'news.scored',
  kafka_group_name = 'clickhouse-sink',
  kafka_format = 'JSONAsString',
  kafka_num_consumers = 1;

-- Parse JSON payload and insert into the analytics table news_scored
CREATE MATERIALIZED VIEW mv_news_scored TO news_scored
AS
SELECT
  toDateTime64(JSONExtractInt(payload, 'scored_ts') / 1000, 3) AS ts,
  JSONExtractString(payload, 'id') AS id,
  JSONExtractString(payload, 'source') AS source,
  JSONExtractString(payload, 'url') AS url,
  JSONExtractString(payload, 'title') AS title,
  JSONExtract(payload, 'tickers', 'Array(String)') AS tickers,
  toFloat32(JSONExtractFloat(payload, 'sentiment')) AS sentiment,
  JSONExtract(payload, 'topics', 'Array(String)') AS topics
FROM news_scored_kafka;
