import React, { useState } from 'react'
import { Newspaper, Search, Activity } from 'lucide-react'

export function Navbar({ onPickTicker }:{ onPickTicker:(t:string)=>void }) {
  const [q,setQ] = useState('')
  return (
    <div className="bg-white border-b border-zinc-200">
      <div className="mx-auto max-w-7xl px-4 py-3 flex items-center gap-4">
        <div className="flex items-center gap-2">
          <Newspaper className="w-5 h-5" />
          <span className="font-semibold">FinNews</span>
        </div>
        <div className="flex-1" />
        <div className="relative">
          <input
            value={q}
            onChange={e=>setQ(e.target.value.toUpperCase())}
            onKeyDown={e=>{ if(e.key==='Enter' && q) onPickTicker(q)}}
            placeholder="Jump to ticker…"
            className="border rounded-xl px-3 py-1.5 text-sm w-48"
          />
          <Search className="w-4 h-4 absolute right-2 top-2.5 text-zinc-400"/>
        </div>
        <div className="hidden md:flex items-center text-xs text-zinc-500 gap-1">
          <Activity className="w-4 h-4"/><span>live</span>
        </div>
      </div>
    </div>
  )
}
