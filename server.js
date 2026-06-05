const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const app = express();

app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.static(__dirname));

const DATA = path.join(__dirname, 'data');
['pings', 'commands', 'live', 'input'].forEach(d => {
  const p = path.join(DATA, d);
  if (!fs.existsSync(p)) fs.mkdirSync(p, { recursive: true });
});

function readJSON(p) {
  try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return null; }
}
function writeJSON(p, data) {
  fs.writeFileSync(p, JSON.stringify(data), 'utf8');
}
function pushKey() {
  const ts = Date.now().toString(36);
  const rand = crypto.randomBytes(8).toString('base64url').slice(0, 8);
  return '-' + ts + rand;
}

// Pings
app.put('/pings/:name.json', (req, res) => {
  const f = path.join(DATA, 'pings', req.params.name + '.json');
  writeJSON(f, req.body);
  res.json(req.body);
});
app.get('/pings.json', (req, res) => {
  const files = fs.readdirSync(path.join(DATA, 'pings')).filter(f => f.endsWith('.json'));
  const out = {};
  files.forEach(f => {
    const name = f.replace('.json', '');
    const data = readJSON(path.join(DATA, 'pings', f));
    if (data) out[name] = data;
  });
  res.json(Object.keys(out).length ? out : null);
});
app.get('/pings/:name.json', (req, res) => {
  const data = readJSON(path.join(DATA, 'pings', req.params.name + '.json'));
  res.json(data);
});
app.delete('/pings/:name.json', (req, res) => {
  const f = path.join(DATA, 'pings', req.params.name + '.json');
  try { fs.unlinkSync(f); } catch {}
  res.json(null);
});

// Commands
app.post('/commands/:name.json', (req, res) => {
  const key = pushKey();
  const dir = path.join(DATA, 'commands', req.params.name);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  writeJSON(path.join(dir, key + '.json'), req.body);
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
  files.forEach(f => {
    const key = f.replace('.json', '');
    const data = readJSON(path.join(dir, f));
    if (data) out[key] = data;
  });
  res.json(Object.keys(out).length ? out : null);
});
app.get('/commands/:name/:id.json', (req, res) => {
  const data = readJSON(path.join(DATA, 'commands', req.params.name, req.params.id + '.json'));
  res.json(data);
});
app.patch('/commands/:name/:id.json', (req, res) => {
  const f = path.join(DATA, 'commands', req.params.name, req.params.id + '.json');
  const existing = readJSON(f) || {};
  Object.assign(existing, req.body);
  writeJSON(f, existing);
  res.json(existing);
});
app.delete('/commands/:name/:id.json', (req, res) => {
  const f = path.join(DATA, 'commands', req.params.name, req.params.id + '.json');
  try { fs.unlinkSync(f); } catch {}
  res.json(null);
});
app.delete('/commands/:name.json', (req, res) => {
  const dir = path.join(DATA, 'commands', req.params.name);
  try { fs.rmSync(dir, { recursive: true, force: true }); } catch {}
  res.json(null);
});

// Live
app.put('/live/:name.json', (req, res) => {
  writeJSON(path.join(DATA, 'live', req.params.name + '.json'), req.body);
  res.json(req.body);
});
app.get('/live/:name.json', (req, res) => {
  const data = readJSON(path.join(DATA, 'live', req.params.name + '.json'));
  res.json(data);
});
app.delete('/live/:name.json', (req, res) => {
  const f = path.join(DATA, 'live', req.params.name + '.json');
  try { fs.unlinkSync(f); } catch {}
  res.json(null);
});

// Input
app.post('/input/:name.json', (req, res) => {
  const f = path.join(DATA, 'input', req.params.name + '.json');
  const existing = readJSON(f) || [];
  const events = Array.isArray(req.body) ? req.body : [req.body];
  existing.push(...events);
  writeJSON(f, existing.slice(-100));
  res.json({ ok: true });
});
app.get('/input/:name.json', (req, res) => {
  const data = readJSON(path.join(DATA, 'input', req.params.name + '.json'));
  res.json(data || []);
});
app.delete('/input/:name.json', (req, res) => {
  writeJSON(path.join(DATA, 'input', req.params.name + '.json'), []);
  res.json(null);
});

// Delete all data for a computer
app.delete('/:name.json', (req, res) => {
  const name = req.params.name;
  ['pings', 'commands', 'live', 'input'].forEach(dir => {
    const p = path.join(DATA, dir, name + '.json');
    try { fs.unlinkSync(p); } catch {}
    const pDir = path.join(DATA, dir, name);
    try { fs.rmSync(pDir, { recursive: true, force: true }); } catch {}
  });
  res.json(null);
});

// Root delete (wipe everything)
app.delete('/.json', (req, res) => {
  ['pings', 'commands', 'live', 'input'].forEach(dir => {
    const p = path.join(DATA, dir);
    if (fs.existsSync(p)) fs.rmSync(p, { recursive: true, force: true });
    fs.mkdirSync(p, { recursive: true });
  });
  res.json(null);
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, '0.0.0.0', () => {
  console.log('Server running on http://0.0.0.0:' + PORT);
});
