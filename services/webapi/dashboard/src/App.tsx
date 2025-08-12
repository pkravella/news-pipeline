import React, { useEffect, useMemo, useState } from 'react'
import { getJSON, sseConnect } from './lib/api'
import { Navbar } from './components/Navbar'
import { LiveTable } from './components/LiveTable'
import { TopMovers } from './components/TopMovers'
import { TickerFeed } from './components/TickerFeed'
import { StatCard } from './components/StatCard'

type Row = {
  ts?: string
  id: string
  source: string
  url: string
  title: string
  tickers: string[]
  sentiment: number
  topics: string[]
  scored_ts?: number
}

export default function App() {
  const [latest, setLatest] = useState<Row[]>([])
  const [stream, setStream] = useState<Row[]>([])
  const [top, setTop] = useState<{positives:any[], negatives:any[], windowMin:number}>({positives:[], negatives:[], windowMin:60})
  const [stats, setStats] = useState<any>({series:[], sources:[], topics:[]})
  const [activeTicker, setActiveTicker] = useState<string>('AAPL')

  // Load initial snapshots
  useEffect(() => {
    getJSON<Row[]>('/api/latest?limit=100').then(setLatest).catch(()=>{})
    getJSON('/api/top-sentiment?window=60').then(setTop).catch(()=>{})
    getJSON('/api/stats').then(setStats).catch(()=>{})
  }, [])

  // SSE stream for live updates
  useEffect(() => {
    const dispose = sseConnect((row) => {
      setStream(s => {
        const next = [row as Row, ...s]
        return next.slice(0, 200)
      })
    })
    return dispose
  }, [])

  const merged = useMemo(() => {
    // combine live stream + initial latest (dedupe by id)
    const map = new Map<string, Row>()
    ;[...stream, ...latest].forEach(r => map.set(r.id, r))
    return Array.from(map.values())
      .sort((a,b) => (b.scored_ts ?? 0) - (a.scored_ts ?? 0))
      .slice(0, 200)
  }, [stream, latest])

  return (
    <div className="min-h-screen">
      <Navbar onPickTicker={setActiveTicker} />
      <div className="mx-auto max-w-7xl px-4 py-6 space-y-6">
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
          <div className="card lg:col-span-2">
            <div className="card-pad">
              <h2 className="text-lg font-semibold mb-2">Live Headlines</h2>
              <LiveTable rows={merged} onPickTicker={setActiveTicker}/>
            </div>
          </div>
          <div className="card">
            <div className="card-pad space-y-3">
              <h2 className="text-lg font-semibold">Top Movers (last {top.windowMin}m)</h2>
              <TopMovers data={top}/>
              <div className="grid grid-cols-2 gap-2">
                <StatCard title="Sources (60m)" items={stats.sources}/>
                <StatCard title="Topics (60m)" items={stats.topics}/>
              </div>
            </div>
          </div>
        </div>

        <div className="card">
          <div className="card-pad">
            <h2 className="text-lg font-semibold mb-2">Ticker Feed — {activeTicker}</h2>
            <TickerFeed symbol={activeTicker}/>
          </div>
        </div>
      </div>
    </div>
  )
}
