import { createClient } from 'redis';
import { cfg } from './config.js';

export const redis = createClient({
  url: `redis://${cfg.redis.host}:${cfg.redis.port}/${cfg.redis.db}`
});

export async function ensureRedis() {
  if (!redis.isOpen) await redis.connect();
}
