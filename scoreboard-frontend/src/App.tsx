import { useEffect, useState } from "react";

type ScoreState = { teamA: number; teamB: number; lastUpdate: number };

export default function App() {
  const [gameStarted, setGameStarted] = useState(false);
  const [score, setScore] = useState<ScoreState>({ teamA: 0, teamB: 0, lastUpdate: Date.now() });

  const intervalId = setInterval(async () => {
    try {
      const res = await fetch("http://localhost:4000/awaitGameStart");
      const data = await res.json();
      console.log("Game started?", data.gameStarted);

      if (data.gameStarted) {
        clearInterval(intervalId); // stop polling
        console.log("Polling stopped, game started!");
        setGameStarted(true);
        console.log("Game started");
      }
    } catch (err) {
      console.error("Error checking game state:", err);
    }
  }, 1000);

  useEffect(() => {

      const interval = setInterval(() => {
        fetch("http://localhost:4000/updateScore")
          .then(res => res.json())
          .then(data => {
            console.log("Score updated:", data);
            setScore({
              teamA: data.teamA,
              teamB: data.teamB,
              lastUpdate: Date.now(),
            });
          })
          .catch(err => console.error("Error updating score:", err));
      }, 1000);


      return () => clearInterval(interval);
    }, []);

  return (
    <body style={{ display: "flex", justifyContent: "center"}}>
      {gameStarted ? (
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
      ) : (
        <p style={{opacity:0.7 }}>
          Game not started yet!
        </p>
      )
      }
    </body>
  );
}
