import { createClient } from '@clickhouse/client';
import { cfg } from './config.js';

export const ch = createClient({
  url: cfg.ch.host,
  username: cfg.ch.username,
  password: cfg.ch.password
});
