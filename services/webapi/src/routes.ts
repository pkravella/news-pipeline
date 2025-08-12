import type { Request, Response } from 'express';
import { ch } from './clickhouse.js';
import { ensureRedis, redis } from './redisClient.js';
import { addClient, removeClient, clientCount } from './sse.js';

export function registerRoutes(app: any) {
  app.get('/api/health', async (_req: Request, res: Response) => {
    try {
      const pong = await ch.ping();
      res.json({ ok: true, clickhouse: pong, sse_clients: clientCount() });
    } catch (e: any) {
      res.status(500).json({ ok: false, error: e?.message });
    }
  });

  app.get('/api/latest', async (req: Request, res: Response) => {
    const limit = Math.min(parseInt(String(req.query.limit ?? '50'), 10), 500);
    const q = `
      SELECT ts, id, source, url, title, tickers, sentiment, topics
      FROM news_scored
      ORDER BY ts DESC
      LIMIT {lim:UInt32}
    `;
    const rows = await ch.query({ query: q, format: 'JSONEachRow', query_params: { lim: limit }});
    const data = await rows.json();
    res.json(data);
  });

  app.get('/api/top-sentiment', async (req: Request, res: Response) => {
    const windowMin = Math.min(parseInt(String(req.query.window ?? '60'), 10), 1440);
    const minCount = Math.min(parseInt(String(req.query.minCount ?? '3'), 10), 100);
    const q = `
      WITH window_start AS (now() - INTERVAL {w:Int32} MINUTE)
      SELECT t as ticker,
             avg(sentiment) AS avg_sent,
             count() AS n
      FROM (SELECT ts, arrayJoin(tickers) AS t, sentiment
            FROM news_scored WHERE ts >= now() - INTERVAL {w:Int32} MINUTE)
      GROUP BY t
      HAVING n >= {m:UInt32}
      ORDER BY avg_sent DESC
      LIMIT 20
    `;
    const qneg = `
      WITH window_start AS (now() - INTERVAL {w:Int32} MINUTE)
      SELECT t as ticker,
             avg(sentiment) AS avg_sent,
             count() AS n
      FROM (SELECT ts, arrayJoin(tickers) AS t, sentiment
            FROM news_scored WHERE ts >= now() - INTERVAL {w:Int32} MINUTE)
      GROUP BY t
      HAVING n >= {m:UInt32}
      ORDER BY avg_sent ASC
      LIMIT 20
    `;
    const [pos, neg] = await Promise.all([
      ch.query({ query: q, format: 'JSONEachRow', query_params: { w: windowMin, m: minCount }}).then(r => r.json()),
      ch.query({ query: qneg, format: 'JSONEachRow', query_params: { w: windowMin, m: minCount }}).then(r => r.json())
    ]);
    res.json({ windowMin, minCount, positives: pos, negatives: neg });
  });

  app.get('/api/ticker/:symbol/feed', async (req: Request, res: Response) => {
    await ensureRedis();
    const symbol = String(req.params.symbol ?? '').toUpperCase();
    const limit = Math.min(parseInt(String(req.query.limit ?? '50'), 10), 200);
    const key = `feed:${symbol}`;
    const rows = await redis.lRange(key, 0, limit - 1);
    const parsed = rows.map(r => JSON.parse(r));
    res.json(parsed);
  });

  app.get('/api/stats', async (_req: Request, res: Response) => {
    const q1 = `
      SELECT toStartOfMinute(ts) AS minute, count() AS n
      FROM news_scored
      WHERE ts >= now() - INTERVAL 60 MINUTE
      GROUP BY minute
      ORDER BY minute ASC
    `;
    const q2 = `
      SELECT source, count() AS n
      FROM news_scored
      WHERE ts >= now() - INTERVAL 60 MINUTE
      GROUP BY source
      ORDER BY n DESC
      LIMIT 10
    `;
    const q3 = `
      SELECT arrayJoin(topics) AS topic, count() AS n
      FROM news_scored
      WHERE ts >= now() - INTERVAL 60 MINUTE
      GROUP BY topic
      ORDER BY n DESC
      LIMIT 15
    `;
    const [series, sources, topics] = await Promise.all([
      ch.query({ query: q1, format: 'JSONEachRow' }).then(r => r.json()),
      ch.query({ query: q2, format: 'JSONEachRow' }).then(r => r.json()),
      ch.query({ query: q3, format: 'JSONEachRow' }).then(r => r.json())
    ]);
    res.json({ series, sources, topics });
  });

  // Server-Sent Events stream from Kafka
  app.get('/api/stream', (req: Request, res: Response) => {
    const id = addClient(res);
    req.on('close', () => removeClient(id));
  });
}
