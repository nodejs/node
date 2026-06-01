'use strict';

// PGO Training Script: HTTP Server Workload
//
// Simulates the most common Node.js use case — an HTTP server handling:
// - JSON REST API requests (GET/POST with JSON bodies)
// - Static content serving (HTML, CSS, JS responses)
// - Chunked transfer encoding
// - Various header patterns (few/many headers, Set-Cookie, CORS)
// - Keep-alive connections with request pipelining
// - URL routing with path parameters and query strings
//
// This exercises: llhttp parser, TCP stack (libuv), header serialization,
// Buffer encoding, URL parsing, JSON parse/stringify, EventEmitter, streams.

const http = require('http');
const { URL } = require('url');
const crypto = require('crypto');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;
const CONCURRENT_CLIENTS = 20;
const PORT = 0; // OS-assigned

// Realistic JSON payloads of varying sizes
const SMALL_JSON = JSON.stringify({ id: 1, name: 'test', active: true });
const MEDIUM_JSON = JSON.stringify({
  users: Array.from({ length: 50 }, (_, i) => ({
    id: i,
    name: `user_${i}`,
    email: `user${i}@example.com`,
    age: 20 + (i % 40),
    active: i % 3 !== 0,
    tags: ['tag1', 'tag2', 'tag3'],
    address: {
      street: `${i * 100} Main St`,
      city: 'Anytown',
      state: 'CA',
      zip: `${90000 + i}`,
    },
  })),
  total: 50,
  page: 1,
  perPage: 50,
});
const LARGE_JSON = JSON.stringify({
  data: Array.from({ length: 500 }, (_, i) => ({
    id: crypto.randomUUID(),
    timestamp: new Date().toISOString(),
    type: ['click', 'view', 'purchase', 'signup'][i % 4],
    properties: {
      source: ['web', 'mobile', 'api'][i % 3],
      browser: ['chrome', 'firefox', 'safari', 'edge'][i % 4],
      os: ['windows', 'macos', 'linux', 'ios', 'android'][i % 5],
      duration: Math.random() * 10000,
      amount: i % 4 === 2 ? (Math.random() * 500).toFixed(2) : undefined,
      items:
        i % 4 === 2 ?
          Array.from({ length: 3 }, (_, j) => ({
            sku: `SKU-${j}-${i}`,
            qty: j + 1,
            price: (Math.random() * 100).toFixed(2),
          })) :
          undefined,
    },
    metadata: {
      ip: `192.168.${i % 256}.${(i * 7) % 256}`,
      userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
      sessionId: crypto.randomUUID(),
    },
  })),
});

// Simulated HTML page
const HTML_PAGE = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>App</title>
  <link rel="stylesheet" href="/styles.css">
</head>
<body>
  <div id="root"></div>
  <script src="/app.js"></script>
</body>
</html>`;

// Simulated CSS
const CSS_CONTENT = `body { margin: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
.container { max-width: 1200px; margin: 0 auto; padding: 0 20px; }
.header { background: #1a1a2e; color: white; padding: 1rem 0; }
.nav { display: flex; gap: 1rem; }
.card { border: 1px solid #ddd; border-radius: 8px; padding: 1rem; margin: 1rem 0; }
.btn { display: inline-block; padding: 0.5rem 1rem; border-radius: 4px; cursor: pointer; }
.btn-primary { background: #0066cc; color: white; }
@media (max-width: 768px) { .container { padding: 0 10px; } }`;

// Simulated JS bundle (minified-ish)
const JS_BUNDLE =
  `!function(){"use strict";` +
  'const e=document.getElementById("root");' +
  'function t(e,t){return Object.assign(document.createElement(e),t)}' +
  Array.from(
    { length: 100 },
    (_, i) =>
      `function c${i}(d){return t("div",{className:"c${i}",textContent:JSON.stringify(d)})}`,
  ).join(';') +
  '}();';

function handleRequest(req, res) {
  const url = new URL(req.url, `http://localhost`);
  const path = url.pathname;
  const method = req.method;

  // CORS headers (very common in REST APIs)
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader(
    'Access-Control-Allow-Methods',
    'GET, POST, PUT, DELETE, OPTIONS',
  );
  res.setHeader('X-Request-Id', crypto.randomUUID());

  if (method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // REST API routes
  if (path === '/api/users' && method === 'GET') {
    const page = url.searchParams.get('page') || '1';
    const limit = url.searchParams.get('limit') || '50';
    res.writeHead(200, {
      'Content-Type': 'application/json',
      'X-Page': page,
      'X-Limit': limit,
      'X-Total': '500',
      'Cache-Control': 'no-cache',
    });
    res.end(MEDIUM_JSON);
    return;
  }

  if (path === '/api/events' && method === 'POST') {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
    });
    req.on('end', () => {
      try {
        const parsed = JSON.parse(body);
        const response = JSON.stringify({
          status: 'accepted',
          count: Array.isArray(parsed) ? parsed.length : 1,
          timestamp: new Date().toISOString(),
        });
        res.writeHead(202, {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(response),
        });
        res.end(response);
      } catch {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end('{"error":"Invalid JSON"}');
      }
    });
    return;
  }

  if (path === '/api/analytics' && method === 'GET') {
    // Chunked response (common for large datasets)
    res.writeHead(200, {
      'Content-Type': 'application/json',
      'Transfer-Encoding': 'chunked',
    });
    res.write(LARGE_JSON.slice(0, 4096));
    res.write(LARGE_JSON.slice(4096, 8192));
    res.write(LARGE_JSON.slice(8192));
    res.end();
    return;
  }

  if (path.startsWith('/api/users/') && method === 'GET') {
    const userId = path.split('/')[3];
    const user = JSON.stringify({
      id: userId,
      name: `User ${userId}`,
      email: `user${userId}@example.com`,
      createdAt: new Date().toISOString(),
      profile: { bio: 'A sample user', avatar: '/img/default.png' },
    });
    res.writeHead(200, {
      'Content-Type': 'application/json',
      'ETag': `"${crypto.createHash('md5').update(user).digest('hex')}"`,
      'Cache-Control': 'max-age=60',
    });
    res.end(user);
    return;
  }

  if (path === '/health') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(SMALL_JSON);
    return;
  }

  // Static content routes
  if (path === '/' || path === '/index.html') {
    res.writeHead(200, {
      'Content-Type': 'text/html; charset=utf-8',
      'Content-Length': Buffer.byteLength(HTML_PAGE),
      'Cache-Control': 'public, max-age=3600',
    });
    res.end(HTML_PAGE);
    return;
  }

  if (path === '/styles.css') {
    res.writeHead(200, {
      'Content-Type': 'text/css',
      'Content-Length': Buffer.byteLength(CSS_CONTENT),
      'Cache-Control': 'public, max-age=86400',
    });
    res.end(CSS_CONTENT);
    return;
  }

  if (path === '/app.js') {
    res.writeHead(200, {
      'Content-Type': 'application/javascript',
      'Content-Length': Buffer.byteLength(JS_BUNDLE),
      'Cache-Control': 'public, max-age=86400',
    });
    res.end(JS_BUNDLE);
    return;
  }

  // Set-Cookie responses (common for auth)
  if (path === '/api/login' && method === 'POST') {
    req.on('data', () => {});
    req.on('end', () => {
      const token = crypto.randomBytes(32).toString('hex');
      res.writeHead(200, {
        'Content-Type': 'application/json',
        'Set-Cookie': [
          `token=${token}; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=3600`,
          `session=${crypto.randomUUID()}; HttpOnly; Path=/`,
        ],
      });
      res.end(JSON.stringify({ token, expiresIn: 3600 }));
    });
    return;
  }

  // 404
  res.writeHead(404, { 'Content-Type': 'application/json' });
  res.end('{"error":"Not Found"}');
}

// Request patterns that clients will cycle through
const REQUEST_PATTERNS = [
  // Weight distribution reflects real API usage
  { method: 'GET', path: '/api/users?page=1&limit=50', weight: 20 },
  {
    method: 'GET',
    path: '/api/users?page=2&limit=25&sort=name&order=asc',
    weight: 10,
  },
  { method: 'GET', path: '/api/users/123', weight: 15 },
  { method: 'GET', path: '/api/users/456', weight: 10 },
  { method: 'GET', path: '/api/analytics', weight: 5 },
  { method: 'GET', path: '/health', weight: 15 },
  { method: 'GET', path: '/', weight: 8 },
  { method: 'GET', path: '/styles.css', weight: 4 },
  { method: 'GET', path: '/app.js', weight: 4 },
  {
    method: 'POST',
    path: '/api/events',
    body: JSON.stringify([
      { type: 'click', target: 'button', timestamp: Date.now() },
      { type: 'view', page: '/dashboard', timestamp: Date.now() },
    ]),
    weight: 5,
  },
  {
    method: 'POST',
    path: '/api/login',
    body: '{"email":"test@example.com","password":"pass123"}',
    weight: 3,
  },
  { method: 'OPTIONS', path: '/api/users', weight: 1 },
];

// Build weighted selection array
const weightedPatterns = [];
for (const pattern of REQUEST_PATTERNS) {
  for (let i = 0; i < pattern.weight; i++) {
    weightedPatterns.push(pattern);
  }
}

function makeRequest(port, pattern) {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: '127.0.0.1',
      port,
      path: pattern.path,
      method: pattern.method,
      headers: {
        'Host': 'localhost',
        'User-Agent': 'PGO-Training/1.0',
        'Accept': 'application/json, text/html',
        'Accept-Encoding': 'identity',
        'Connection': 'keep-alive',
      },
    };

    if (pattern.body) {
      options.headers['Content-Type'] = 'application/json';
      options.headers['Content-Length'] = Buffer.byteLength(pattern.body);
    }

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', (chunk) => {
        data += chunk;
      });
      res.on('end', () =>
        resolve({ status: res.statusCode, size: data.length }),
      );
    });
    req.on('error', reject);
    if (pattern.body) req.write(pattern.body);
    req.end();
  });
}

async function runClient(port, endTime) {
  let requests = 0;
  const agent = new http.Agent({ keepAlive: true, maxSockets: 2 });

  while (Date.now() < endTime) {
    const pattern =
      weightedPatterns[Math.floor(Math.random() * weightedPatterns.length)];
    try {
      await makeRequest(port, pattern);
      requests++;
    } catch {
      // Instrumented builds are slow; brief pause on error
      await new Promise((r) => setTimeout(r, 10));
    }
  }
  agent.destroy();
  return requests;
}

async function main() {
  const server = http.createServer(handleRequest);

  await new Promise((resolve) => server.listen(PORT, '127.0.0.1', resolve));
  const port = server.address().port;
  console.log(`[pgo-http-server] Server listening on port ${port}`);

  const endTime = Date.now() + DURATION_MS;
  const clients = Array.from({ length: CONCURRENT_CLIENTS }, () =>
    runClient(port, endTime),
  );
  const results = await Promise.all(clients);
  const totalRequests = results.reduce((a, b) => a + b, 0);

  server.close();
  console.log(
    `[pgo-http-server] Completed ${totalRequests} requests in ${DURATION_MS / 1000}s (${(totalRequests / (DURATION_MS / 1000)).toFixed(0)} req/s)`,
  );
}

main().catch((err) => {
  console.error('[pgo-http-server] Error:', err);
  process.exit(1);
});
