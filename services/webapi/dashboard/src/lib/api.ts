const API = import.meta.env.VITE_API_BASE ?? 'http://localhost:8080';

export async function getJSON<T>(path: string): Promise<T> {
  const r = await fetch(`${API}${path}`);
  if (!r.ok) throw new Error(`HTTP ${r.status}`);
  return r.json();
}

export function sseConnect(onData: (row: any) => void) {
  const es = new EventSource(`${API}/api/stream`);
  es.addEventListener('scored', (e: MessageEvent) => {
    try { onData(JSON.parse(e.data)); } catch {}
  });
  return () => es.close();
}
