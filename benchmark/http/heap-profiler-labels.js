'use strict';

// Benchmark: HTTP server throughput impact of heap profiler with labels.
//
// Measures requests/sec across three modes:
// - none: no profiler (baseline)
// - sampling: profiler active, no labels
// - sampling-with-labels: profiler active with labels via withHeapProfileLabels
//
// Workload per request: ~100KB V8 heap (JSON parse/stringify) + ~50KB Buffer
// to exercise both HeapProfileLabelsCallback and ProfilingArrayBufferAllocator.
//
// Run with compare.js:
//   node benchmark/compare.js --old ./out/Release/node --new ./out/Release/node \
//     --runs 10 --filter heap-profiler-labels --set c=50 -- http

const common = require('../common.js');
const { PORT } = require('../_http-benchmarkers.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  mode: ['none', 'sampling', 'sampling-with-labels'],
  c: [50],
  duration: 10,
});

// Build a ~100KB realistic JSON payload template (API response shape).
const items = [];
for (let i = 0; i < 200; i++) {
  items.push({
    id: i,
    name: `user-${i}`,
    email: `user${i}@example.com`,
    role: 'admin',
    metadata: { created: '2024-01-01', tags: ['a', 'b', 'c'] },
  });
}
const payloadTemplate = JSON.stringify({ data: items, total: 200 });

function main({ mode, c, duration }) {
  const http = require('http');

  const interval = 512 * 1024; // 512KB — V8 default, production-realistic.

  if (mode !== 'none') {
    v8.startSamplingHeapProfiler(interval);
  }

  const server = http.createServer((req, res) => {
    const handler = () => {
      // Realistic mixed workload:
      // 1. ~100KB V8 heap: JSON parse + stringify (simulates API response building)
      const parsed = JSON.parse(payloadTemplate);
      parsed.requestId = Math.random();
      const body = JSON.stringify(parsed);

      // 2. ~50KB Buffer (simulates response buffering / crypto / compression)
      const buf = Buffer.alloc(50 * 1024, 0x42);

      // Keep buf reference alive until response is sent.
      res.writeHead(200, {
        'Content-Type': 'application/json',
        'Content-Length': body.length,
        'X-Buf-Check': buf[0],
      });
      res.end(body);
    };

    if (mode === 'sampling-with-labels') {
      v8.withHeapProfileLabels({ route: req.url }, handler);
    } else {
      handler();
    }
  });

  server.listen(PORT, () => {
    bench.http({
      path: '/api/bench',
      connections: c,
      duration,
    }, () => {
      if (mode !== 'none') {
        v8.stopSamplingHeapProfiler();
      }
      server.close();
    });
  });
}
