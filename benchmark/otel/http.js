'use strict';

// Measures the per-request overhead of the built-in OpenTelemetry tracing
// subsystem on the http hot path. Requests are issued sequentially over a
// single keep-alive connection so that the difference between the traced
// and untraced configurations reflects per-request work (two spans per
// request: one SERVER, one CLIENT), not scheduling or connection noise.
//
// When tracing is enabled, spans are exported to an unreachable endpoint:
// the measurement covers span creation and serialization but not collector
// latency. Export failures are reported as throttled warnings.
//
// Run with: node benchmark/otel/http.js

const common = require('../common');

const bench = common.createBenchmark(main, {
  tracing: [0, 1],
  n: [1e5],
}, {
  flags: ['--expose-internals'],
});

function main({ tracing, n }) {
  if (tracing) {
    const otel = require('internal/otel/core');
    otel.start({ endpoint: 'http://127.0.0.1:1' });
  }

  const http = require('http');

  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end('ok');
  });

  server.listen(0, '127.0.0.1', () => {
    const port = server.address().port;
    const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });

    let completed = 0;
    let measured = false;
    const kWarmup = 100;

    function request(done) {
      http.get({ host: '127.0.0.1', port, agent, path: '/bench' }, (res) => {
        res.resume();
        res.on('end', done);
      });
    }

    request(function done() {
      if (!measured) {
        completed++;
        if (completed < kWarmup) {
          request(done);
          return;
        }
        // Warmup is done; the measured requests start now.
        measured = true;
        completed = 0;
        bench.start();
      } else {
        completed++;
      }
      if (completed >= n) {
        bench.end(n);
        server.close();
        agent.destroy();
        return;
      }
      request(done);
    });
  });
}
