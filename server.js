const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const http = require('http');
const { WebSocketServer } = require('ws');
const app = express();

app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.static(__dirname));

const DATA = path.join(__dirname, 'data');
['pings', 'commands', 'live', 'input', 'logs'].forEach(d => {
  const p = path.join(DATA, d);
  if (!fs.existsSync(p)) fs.mkdirSync(p, { recursive: true });
});

const activityLog = [];
const MAX_LOG = 500;
function log(type, computer, detail) {
  const entry = { t: new Date().toISOString(), type, computer, detail: (detail||'').slice(0, 500) };
  activityLog.push(entry);
  if (activityLog.length > MAX_LOG) activityLog.splice(0, activityLog.length - MAX_LOG);
  broadcast({ event: 'log', data: entry });
}

const rateLimits = {};
function rateLimit(ip, limit = 100, windowMs = 1000) {
  const now = Date.now();
  if (!rateLimits[ip]) rateLimits[ip] = [];
  rateLimits[ip] = rateLimits[ip].filter(t => now - t < windowMs);
  if (rateLimits[ip].length >= limit) return false;
  rateLimits[ip].push(now);
  return true;
}
setInterval(() => { const now = Date.now(); for (const ip in rateLimits) { rateLimits[ip] = rateLimits[ip].filter(t => now - t < 1000); if (!rateLimits[ip].length) delete rateLimits[ip]; } }, 10000);

function readJSON(p) { try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return null; } }
function writeJSON(p, data) { fs.writeFileSync(p, JSON.stringify(data), 'utf8'); }
function pushKey() { return '-' + Date.now().toString(36) + crypto.randomBytes(6).toString('base64url'); }

const server = http.createServer(app);
const wss = new WebSocketServer({ server });
const clients = new Set();
wss.on('connection', ws => { clients.add(ws); ws.on('close', () => clients.delete(ws)); });
function broadcast(msg) { const data = JSON.stringify(msg); for (const ws of clients) { if (ws.readyState === 1) ws.send(data); } }

function cleanupStale() {
  try {
    const now = Date.now();
    const cmdDir = path.join(DATA, 'commands');
    if (fs.existsSync(cmdDir)) {
      fs.readdirSync(cmdDir).forEach(name => {
        const dir = path.join(cmdDir, name);
        if (!fs.existsSync(dir) || !fs.statSync(dir).isDirectory()) return;
        fs.readdirSync(dir).forEach(f => {
          const fp = path.join(dir, f);
          const stat = fs.statSync(fp);
          if (now - stat.mtimeMs > 300000) { try { fs.unlinkSync(fp); } catch {} }
        });
        if (fs.readdirSync(dir).length === 0) { try { fs.rmdirSync(dir); } catch {} }
      });
    }
    const pingDir = path.join(DATA, 'pings');
    if (fs.existsSync(pingDir)) {
      fs.readdirSync(pingDir).forEach(f => {
        const fp = path.join(pingDir, f);
        const data = readJSON(fp);
        if (data && data.created_at && now - new Date(data.created_at).getTime() > 86400000) {
          try { fs.unlinkSync(fp); } catch {}
        }
      });
    }
  } catch {}
}
setInterval(cleanupStale, 60000);

// Health check
app.get('/health.json', (req, res) => {
  const pings = fs.readdirSync(path.join(DATA, 'pings')).filter(f => f.endsWith('.json'));
  const now = Date.now();
  const online = pings.filter(f => {
    const d = readJSON(path.join(DATA, 'pings', f));
    return d && d.created_at && now - new Date(d.created_at).getTime() < 60000;
  });
  res.json({ status: 'ok', clients: pings.length, online: online.length, uptime: process.uptime() });
});

// Activity log
app.get('/logs.json', (req, res) => {
  const limit = Math.min(parseInt(req.query.limit) || 100, MAX_LOG);
  res.json(activityLog.slice(-limit));
});

// Pings
app.put('/pings/:name.json', (req, res) => {
  const f = path.join(DATA, 'pings', req.params.name + '.json');
  const old = readJSON(f);
  writeJSON(f, req.body);
  if (!old) log('online', req.params.name, 'Client connected');
  broadcast({ event: 'ping', computer: req.params.name, data: req.body });
  res.json(req.body);
});
app.get('/pings.json', (req, res) => {
  const files = fs.readdirSync(path.join(DATA, 'pings')).filter(f => f.endsWith('.json'));
  const out = {};
  files.forEach(f => { const name = f.replace('.json', ''); const data = readJSON(path.join(DATA, 'pings', f)); if (data) out[name] = data; });
  res.json(Object.keys(out).length ? out : null);
});
app.get('/pings/:name.json', (req, res) => {
  res.json(readJSON(path.join(DATA, 'pings', req.params.name + '.json')));
});
app.delete('/pings/:name.json', (req, res) => {
  try { fs.unlinkSync(path.join(DATA, 'pings', req.params.name + '.json')); } catch {}
  log('offline', req.params.name, 'Client removed');
  res.json(null);
});

// Commands
app.post('/commands/:name.json', (req, res) => {
  if (!rateLimit(req.ip, 50)) return res.status(429).json({ error: 'rate_limited' });
  const key = pushKey();
  const dir = path.join(DATA, 'commands', req.params.name);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  const cmd = { ...req.body, created_at: new Date().toISOString() };
  writeJSON(path.join(dir, key + '.json'), cmd);
  log('command', req.params.name, req.body.type || 'unknown');
  broadcast({ event: 'command', computer: req.params.name, id: key, data: cmd });
  res.json({ name: key });
});
app.put('/commands/:name/:id.json', (req, res) => {
  const dir = path.join(DATA, 'commands', req.params.name);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  writeJSON(path.join(dir, req.params.id + '.json'), req.body);
  res.json(req.body);
});
app.get('/commands/:name.json', (req, res) => {
  const dir = path.join(DATA, 'commands', req.params.name);
  if (!fs.existsSync(dir)) return res.json(null);
  const files = fs.readdirSync(dir).filter(f => f.endsWith('.json'));
  const out = {};
  files.forEach(f => { const key = f.replace('.json', ''); const data = readJSON(path.join(dir, f)); if (data) out[key] = data; });
  res.json(Object.keys(out).length ? out : null);
});
app.get('/commands/:name/:id.json', (req, res) => {
  res.json(readJSON(path.join(DATA, 'commands', req.params.name, req.params.id + '.json')));
});
app.patch('/commands/:name/:id.json', (req, res) => {
  const f = path.join(DATA, 'commands', req.params.name, req.params.id + '.json');
  const existing = readJSON(f) || {};
  Object.assign(existing, req.body);
  writeJSON(f, existing);
  if (req.body.status === 'done') {
    log('result', req.params.name, (existing.type || '') + (req.body.result ? ': ' + req.body.result.slice(0, 200) : ''));
    broadcast({ event: 'result', computer: req.params.name, id: req.params.id, data: existing });
  }
  res.json(existing);
});
app.delete('/commands/:name/:id.json', (req, res) => {
  try { fs.unlinkSync(path.join(DATA, 'commands', req.params.name, req.params.id + '.json')); } catch {}
  res.json(null);
});
app.delete('/commands/:name.json', (req, res) => {
  try { fs.rmSync(path.join(DATA, 'commands', req.params.name), { recursive: true, force: true }); } catch {}
  res.json(null);
});

// Live
app.put('/live/:name.json', (req, res) => {
  writeJSON(path.join(DATA, 'live', req.params.name + '.json'), req.body);
  broadcast({ event: 'live', computer: req.params.name, q: req.body.q, scale: req.body.scale });
  res.json(req.body);
});
app.get('/live/:name.json', (req, res) => {
  res.json(readJSON(path.join(DATA, 'live', req.params.name + '.json')));
});
app.delete('/live/:name.json', (req, res) => {
  try { fs.unlinkSync(path.join(DATA, 'live', req.params.name + '.json')); } catch {}
  res.json(null);
});

// Input
app.post('/input/:name.json', (req, res) => {
  const f = path.join(DATA, 'input', req.params.name + '.json');
  const existing = readJSON(f) || [];
  const events = Array.isArray(req.body) ? req.body : [req.body];
  existing.push(...events);
  writeJSON(f, existing.slice(-200));
  res.json({ ok: true });
});
app.get('/input/:name.json', (req, res) => {
  res.json(readJSON(path.join(DATA, 'input', req.params.name + '.json')) || []);
});
app.delete('/input/:name.json', (req, res) => {
  writeJSON(path.join(DATA, 'input', req.params.name + '.json'), []);
  res.json(null);
});

// Delete all data for a computer
app.delete('/:name.json', (req, res) => {
  const name = req.params.name;
  ['pings', 'commands', 'live', 'input'].forEach(dir => {
    try { fs.unlinkSync(path.join(DATA, dir, name + '.json')); } catch {}
    try { fs.rmSync(path.join(DATA, dir, name), { recursive: true, force: true }); } catch {}
  });
  log('removed', name, 'All data deleted');
  res.json(null);
});

// Root delete
app.delete('/.json', (req, res) => {
  ['pings', 'commands', 'live', 'input'].forEach(dir => {
    const p = path.join(DATA, dir);
    if (fs.existsSync(p)) fs.rmSync(p, { recursive: true, force: true });
    fs.mkdirSync(p, { recursive: true });
  });
  log('system', 'all', 'All data wiped');
  res.json(null);
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log('Server v2 running on http://0.0.0.0:' + PORT);
  console.log('WebSocket: ws://0.0.0.0:' + PORT);
  log('system', 'server', 'Server started on port ' + PORT);
});
