import React from 'react'

function sentimentColor(s:number) {
  if (s >= 0.4) return 'text-green-600'
  if (s <= -0.4) return 'text-red-600'
  return 'text-zinc-700'
}

export function LiveTable({ rows, onPickTicker }:{
  rows: {id:string; source:string; url:string; title:string; tickers:string[]; sentiment:number; topics:string[]; scored_ts?:number}[],
  onPickTicker:(t:string)=>void
}) {
  return (
    <div className="overflow-auto">
      <table className="min-w-full text-sm">
        <thead className="text-left text-xs text-zinc-500">
          <tr>
            <th className="py-2 pr-2">Time</th>
            <th className="py-2 pr-2">Title</th>
            <th className="py-2 pr-2">Tickers</th>
            <th className="py-2 pr-2">Sentiment</th>
            <th className="py-2 pr-2">Topics</th>
            <th className="py-2 pr-2">Source</th>
          </tr>
        </thead>
        <tbody>
          {rows.map(r => (
            <tr key={r.id} className="border-t">
              <td className="py-2 pr-2 text-zinc-600">{r.scored_ts ? new Date(r.scored_ts).toLocaleTimeString() : ''}</td>
              <td className="py-2 pr-2">
                <a href={r.url} target="_blank" className="text-blue-700 hover:underline">{r.title}</a>
              </td>
              <td className="py-2 pr-2">
                <div className="flex gap-1 flex-wrap">
                  {r.tickers.map(t => (
                    <button key={t} onClick={()=>onPickTicker(t)} className="badge">{t}</button>
                  ))}
                </div>
              </td>
              <td className={`py-2 pr-2 font-medium ${sentimentColor(r.sentiment)}`}>{r.sentiment.toFixed(2)}</td>
              <td className="py-2 pr-2">
                <div className="flex gap-1 flex-wrap">
                  {r.topics.map(t => <span key={t} className="badge">{t}</span>)}
                </div>
              </td>
              <td className="py-2 pr-2 text-zinc-600">{r.source}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}
