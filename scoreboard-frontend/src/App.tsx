import { useEffect, useState } from "react";
import { io, Socket } from "socket.io-client";

type ScoreState = { teamA: number; teamB: number; lastUpdate: number };

export default function App() {
  const [score, setScore] = useState<ScoreState>({ teamA: 0, teamB: 0, lastUpdate: Date.now() });

  useEffect(() => {
    let s: Socket | null = io("http://localhost:4000");

    s.on("connect", () => {
      // Initial fetch in case we connected after some events
      fetch("http://localhost:4000/state").then(r => r.json()).then(setScore).catch(()=>{});
    });

    s.on("score:update", (data: ScoreState) => setScore(data));

    return () => { s?.close(); s = null; };
  }, []);

  return (
    <main style={{ fontFamily: "system-ui, sans-serif", maxWidth: 520, margin: "4rem auto", textAlign: "center" }}>
      <h1>Territory Game Scoreboard</h1>
      <div style={{ display:"grid", gridTemplateColumns:"1fr 1fr", gap:16, marginTop:24 }}>
        <section style={{ padding:24, border:"1px solid #444", borderRadius:16 }}>
          <h2>Team A</h2>
          <p style={{ fontSize:56, margin:0 }}>{score.teamA}</p>
        </section>
        <section style={{ padding:24, border:"1px solid #444", borderRadius:16 }}>
          <h2>Team B</h2>
          <p style={{ fontSize:56, margin:0 }}>{score.teamB}</p>
        </section>
      </div>
      <p style={{ marginTop:24, opacity:0.7 }}>
        Last update: {new Date(score.lastUpdate).toLocaleTimeString()}
      </p>
    </main>
  );
}
