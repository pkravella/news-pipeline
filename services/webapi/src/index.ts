import express from 'express';
import cors from 'cors';
import { cfg } from './config.js';
import { registerRoutes } from './routes.js';
import { startKafkaSSE } from './kafka.js';

async function main() {
  const app = express();
  app.use(cors({ origin: cfg.corsOrigin, credentials: false }));
  app.use(express.json({ limit: '1mb' }));

  registerRoutes(app);

  app.listen(cfg.port, () => {
    console.log(`[webapi] listening on http://localhost:${cfg.port}`);
  });

  await startKafkaSSE();
  console.log(`[webapi] Kafka SSE bridge connected.`);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
