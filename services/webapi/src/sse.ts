import { Response } from 'express';

type Client = { id: number; res: Response };
let nextId = 1;
const clients: Client[] = [];

export function addClient(res: Response): number {
  const id = nextId++;
  clients.push({ id, res });
  res.writeHead(200, {
    'Content-Type': 'text/event-stream',
    'Cache-Control': 'no-cache',
    Connection: 'keep-alive',
    'Access-Control-Allow-Origin': '*'
  });
  res.write(`event: ping\ndata: ${Date.now()}\n\n`);
  return id;
}

export function removeClient(id: number) {
  const idx = clients.findIndex(c => c.id === id);
  if (idx >= 0) {
    try { clients[idx].res.end(); } catch {}
    clients.splice(idx, 1);
  }
}

export function broadcast(event: string, data: unknown) {
  const payload = `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
  for (const c of clients) {
    try { c.res.write(payload); } catch {}
  }
}

export function clientCount() { return clients.length; }
