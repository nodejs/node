'use strict';

// Benchmark: realistic app-server + DB-server heap profiler overhead.
//
// Architecture: wrk → [App Server :PORT] → [DB Server :PORT+1]
//
// The app server fetches JSON rows from the DB server, parses,
// sums two columns over all rows, and returns the result. This exercises:
//   - http.get (async I/O + Buffer allocation for response body)
//   - JSON.parse of realistic DB response (V8 heap allocation)
//   - Two iteration passes over rows (intermediate values)
//   - ALS label propagation across async I/O boundary
//
// Run with compare.js for statistical significance:
//   node benchmark/compare.js --old ./out/Release/node --new ./out/Release/node \
//     --runs 30 --filter heap-profiler-realistic --set rows=1000 -- http

const common = require('../common.js');
const { PORT } = require('../_http-benchmarkers.js');
const v8 = require('v8');
const http = require('http');

const DB_PORT = PORT + 1;

const bench = common.createBenchmark(main, {
  mode: ['none', 'sampling', 'sampling-with-labels'],
  rows: [100, 1000],
  c: [50],
  duration: 10,
});

// --- DB Server: pre-built JSON responses keyed by row count ---

function buildDBResponse(n) {
  const categories = ['electronics', 'clothing', 'food', 'books', 'tools'];
  const rows = [];
  for (let i = 0; i < n; i++) {
    rows.push({
      id: i,
      amount: Math.round(Math.random() * 10000) / 100,
      quantity: Math.floor(Math.random() * 500),
      name: `user-${String(i).padStart(6, '0')}`,
      email: `user${i}@example.com`,
      category: categories[i % categories.length],
    });
  }
  const body = JSON.stringify({ rows, total: n });
  return { body, len: Buffer.byteLength(body) };
}

// --- App Server helpers ---

function fetchFromDB(rows) {
  return new Promise((resolve, reject) => {
    const req = http.get(
      `http://127.0.0.1:${DB_PORT}/?rows=${rows}`,
      (res) => {
        const chunks = [];
        res.on('data', (chunk) => chunks.push(chunk));
        res.on('end', () => {
          try {
            resolve(JSON.parse(Buffer.concat(chunks).toString()));
          } catch (e) {
            reject(e);
          }
        });
      },
    );
    req.on('error', reject);
  });
}

function processRows(data) {
  const { rows } = data;
  // Two passes — simulates light business logic (column aggregation).
  let totalAmount = 0;
  for (let i = 0; i < rows.length; i++) {
    totalAmount += rows[i].amount;
  }
  let totalQuantity = 0;
  for (let i = 0; i < rows.length; i++) {
    totalQuantity += rows[i].quantity;
  }
  return {
    totalAmount: Math.round(totalAmount * 100) / 100,
    totalQuantity,
    count: rows.length,
  };
}

function main({ mode, rows, c, duration }) {
  // Pre-build DB responses.
  const dbResponses = {};
  for (const n of [100, 1000]) {
    dbResponses[n] = buildDBResponse(n);
  }

  // Start DB server.
  const dbServer = http.createServer((req, res) => {
    const url = new URL(req.url, `http://127.0.0.1:${DB_PORT}`);
    const n = parseInt(url.searchParams.get('rows') || '1000', 10);
    const resp = dbResponses[n] || dbResponses[1000];
    res.writeHead(200, {
      'Content-Type': 'application/json',
      'Content-Length': resp.len,
    });
    res.end(resp.body);
  });

  dbServer.listen(DB_PORT, () => {
    const interval = 512 * 1024;
    if (mode !== 'none') {
      v8.startSamplingHeapProfiler(interval);
    }

    // Start app server.
    const appServer = http.createServer((req, res) => {
      const handler = async () => {
        const data = await fetchFromDB(rows);
        const result = processRows(data);
        const body = JSON.stringify(result);
        res.writeHead(200, {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(body),
        });
        res.end(body);
      };

      if (mode === 'sampling-with-labels') {
        v8.withHeapProfileLabels({ route: req.url }, handler);
      } else {
        handler();
      }
    });

    appServer.listen(PORT, () => {
      bench.http({
        path: '/api/data',
        connections: c,
        duration,
      }, () => {
        if (mode !== 'none') {
          v8.stopSamplingHeapProfiler();
        }
        appServer.close();
        dbServer.close();
      });
    });
  });
}
