// Flags: --expose-gc
// Exercise labelled Buffer allocation and off-thread backing-store cleanup
// under sustained HTTP traffic and concurrent ArrayBuffer sweeping.
'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const v8 = require('v8');

// Buffer sizes well above Buffer.poolSize/2 (4KB) so allocUnsafe goes
// through the V8 ArrayBufferAllocator and produces sweep-eligible
// BackingStores. Cycling sizes spreads allocations across young/old
// generations and gives the sweeper a steady stream of work.
const BUFFER_SIZES = [64 * 1024, 128 * 1024, 256 * 1024];

const TRAFFIC_DURATION_MS = 3000;
const CONCURRENCY = 60;
const HARD_DEADLINE_MS = 13_000;

function makeHandler() {
  let i = 0;
  return function handler(req, res) {
    const size = BUFFER_SIZES[i % BUFFER_SIZES.length];
    const route = `r${i % 10}`;
    i++;

    v8.withHeapProfileLabels({ route }, () => {
      // Allocate a sweep-eligible Buffer per request. The Buffer becomes
      // garbage as soon as the response ends, putting pressure on the
      // ArrayBufferSweeper.
      const buf = Buffer.allocUnsafe(size);
      buf.fill(0);
      res.writeHead(200, {
        'Content-Type': 'application/octet-stream',
        'Content-Length': String(buf.length),
      });
      res.end(buf);
    });
  };
}

function driveTraffic(port, done) {
  let started = 0;
  let finished = 0;
  let errored = 0;
  let stopping = false;
  const startTime = Date.now();
  const stopAt = startTime + TRAFFIC_DURATION_MS;

  function maybeStop() {
    if (Date.now() >= stopAt) stopping = true;
    if (stopping && started === finished + errored) {
      done({ started, finished, errored });
    } else if (!stopping) {
      kick();
    }
  }

  function kick() {
    while (!stopping &&
           started - (finished + errored) < CONCURRENCY) {
      started++;
      const req = http.get({ port, path: '/' }, (res) => {
        res.on('data', () => {});
        res.on('end', () => {
          finished++;
          // Keep the ArrayBufferSweeper active during the traffic burst.
          if (finished % 100 === 0 && global.gc) global.gc();
          maybeStop();
        });
        res.on('error', () => {
          errored++;
          maybeStop();
        });
      });
      req.on('error', () => {
        errored++;
        maybeStop();
      });
    }
  }

  kick();
}

const server = http.createServer(makeHandler());

// Hard deadline to keep total wall time bounded even if traffic stalls.
const deadline = setTimeout(() => {
  assert.fail(`Test exceeded ${HARD_DEADLINE_MS}ms wall-time deadline`);
}, HARD_DEADLINE_MS);
deadline.unref();

const handle = v8.startHeapProfile({
  sampleInterval: 65536,
  stackDepth: 16,
  includeObjectsCollectedByMajorGC: true,
  includeObjectsCollectedByMinorGC: true,
  labels: true,
});

server.listen(0, common.mustCall(() => {
  const { port } = server.address();
  driveTraffic(port, common.mustCall(({ finished, errored }) => {
    try {
      assert.ok(finished > 0,
                `Expected some successful requests, got ${finished} ` +
                `(errored=${errored})`);

      // Exercise label resolution after concurrent backing-store cleanup.
      const profile = handle.getAllocationProfile();
      assert.ok(profile, 'getAllocationProfile returned no profile');
      assert.ok(Array.isArray(profile.samples),
                'profile.samples should be an array');
    } finally {
      handle.stop();
      server.close();
      clearTimeout(deadline);
    }
  }));
}));
