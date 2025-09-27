const express = require("express");
const http = require("http");
const cors = require("cors");
const bodyParser = require("body-parser");
const { Server } = require("socket.io");

const app = express();
app.use(cors());
app.use(bodyParser.json());

const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

const state = { teamA: 0, teamB: 0, lastUpdate: Date.now() };

function broadcast() {
  io.emit("score:update", state);
}

app.get("/health", (_, res) => res.json({ ok: true }));
app.get("/state", (_, res) => res.json(state));

// Arduino POSTS here with JSON like: { type:"delta", team:"A", value:1 } or { type:"snapshot", teamA:3, teamB:2 }
app.post("/ingest", (req, res) => {
  try {
    const d = req.body || {};
    if (d.type === "snapshot") {
      state.teamA = Number(d.teamA || 0);
      state.teamB = Number(d.teamB || 0);
    } else if (d.type === "delta" && (d.team === "A" || d.team === "B")) {
      const key = d.team === "A" ? "teamA" : "teamB";
      if (typeof d.value === "number") state[key] = d.value;
      else state[key] += 1; // if no value provided, treat as +1
    }
    state.lastUpdate = Date.now();
    broadcast();
    res.json({ ok: true });
  } catch (e) {
    res.status(400).json({ ok: false, error: String(e) });
  }
});

const PORT = process.env.PORT || 4000;
server.listen(PORT, () => console.log(`Backend on http://localhost:${PORT}`));
