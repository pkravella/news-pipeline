import React from 'react'

export function StatCard({ title, items }:{ title:string; items:any[] }) {
  return (
    <div className="card">
      <div className="card-pad">
        <div className="text-sm font-medium mb-2">{title}</div>
        <ul className="space-y-1 text-sm">
          {items?.slice(0,8).map((x:any) => {
            const key = x.source ?? x.topic ?? 'n/a'
            const n = x.n ?? 0
            return <li key={key} className="flex justify-between"><span className="text-zinc-700">{key}</span><span className="text-zinc-600">{n}</span></li>
          })}
        </ul>
      </div>
    </div>
  )
}
