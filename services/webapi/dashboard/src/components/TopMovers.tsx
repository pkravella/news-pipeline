import React from 'react'

export function TopMovers({ data }:{ data:{ windowMin:number, positives:any[], negatives:any[] } }) {
  return (
    <div className="space-y-4">
      <section>
        <h3 className="text-sm font-medium mb-1">Most Positive</h3>
        <ul className="space-y-1">
          {data.positives?.slice(0,10).map((r:any) => (
            <li key={r.ticker} className="flex items-center justify-between text-sm">
              <span className="font-medium text-green-700">{r.ticker}</span>
              <span className="text-zinc-600">{Number(r.avg_sent).toFixed(2)} · n={r.n}</span>
            </li>
          ))}
        </ul>
      </section>
      <section>
        <h3 className="text-sm font-medium mb-1">Most Negative</h3>
        <ul className="space-y-1">
          {data.negatives?.slice(0,10).map((r:any) => (
            <li key={r.ticker} className="flex items-center justify-between text-sm">
              <span className="font-medium text-red-700">{r.ticker}</span>
              <span className="text-zinc-600">{Number(r.avg_sent).toFixed(2)} · n={r.n}</span>
            </li>
          ))}
        </ul>
      </section>
    </div>
  )
}
