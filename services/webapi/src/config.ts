import 'dotenv/config';

export const cfg = {
  port: parseInt(process.env.PORT ?? '8080', 10),
  corsOrigin: process.env.CORS_ORIGIN ?? 'http://localhost:5173',
  ch: {
    host: process.env.CH_HOST ?? 'http://localhost:8123',
    username: process.env.CH_USER ?? 'default',
    password: process.env.CH_PASSWORD ?? ''
  },
  redis: {
    host: process.env.REDIS_HOST ?? 'localhost',
    port: parseInt(process.env.REDIS_PORT ?? '6379', 10),
    db: parseInt(process.env.REDIS_DB ?? '0', 10)
  },
  kafka: {
    brokers: (process.env.KAFKA_BROKERS ?? 'localhost:9092').split(','),
    scoredTopic: process.env.KAFKA_SCORED_TOPIC ?? 'news.scored',
    groupSse: process.env.KAFKA_GROUP_SSE ?? 'webapi-sse'
  }
};
