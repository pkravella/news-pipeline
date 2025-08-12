import React, { useEffect, useState } from 'react'
import { getJSON } from '../lib/api'

type Row = { id:string; source:string; url:string; title:string; tickers:string[]; sentiment:number; topics:string[]; scored_ts:number }

export function TickerFeed({ symbol }:{ symbol:string }) {
  const [rows, setRows] = useState<Row[]>([])
  useEffect(() => {
    if (!symbol) return
    getJSON<Row[]>(`/api/ticker/${encodeURIComponent(symbol)}/feed?limit=50`)
      .then(setRows).catch(()=>setRows([]))
  }, [symbol])

  return (
    <div className="space-y-2">
      {rows.map(r => (
        <div className="border-b pb-2" key={r.id}>
          <div className="text-xs text-zinc-500">{new Date(r.scored_ts).toLocaleString()} · {r.source}</div>
          <a href={r.url} target="_blank" className="text-sm text-blue-700 hover:underline">{r.title}</a>
          <div className="mt-1 text-xs">
            <span className={`badge ${r.sentiment>=0?'border-green-300 text-green-700':'border-red-300 text-red-700'}`}>sent {r.sentiment.toFixed(2)}</span>
            {r.topics.map(t => <span key={t} className="badge ml-1">{t}</span>)}
          </div>
        </div>
      ))}
      {rows.length===0 && <div className="text-sm text-zinc-500">No recent items.</div>}
    </div>
  )
}
