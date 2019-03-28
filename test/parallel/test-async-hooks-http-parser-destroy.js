'use strict';
require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const http = require('http');

// Regression test for https://github.com/nodejs/node/issues/19859.
// Checks that matching destroys are emitted when creating new/reusing old http
// parser instances.

const N = 50;
const KEEP_ALIVE = 100;

// XXX (trott): 'destroy' event can happen more than once for some asyncIds.
// Using a Set for now to work around the bug, but it can be simplified perhaps
// once https://github.com/nodejs/node/issues/26961 is fixed.
const createdIds = new Set();
const destroyedIds = new Set();
async_hooks.createHook({
  init: (asyncId, type) => {
    if (type === 'HTTPPARSER') {
      createdIds.add(asyncId);
    }
  },
  destroy: (asyncId) => {
    if (createdIds.has(asyncId)) {
      destroyedIds.add(asyncId);
      // Each HTTP request results in two createdIds and two destroyedIds.
      if (destroyedIds.size === N * 2)
        server.close();
    }
  }
}).enable();

const server = http.createServer((req, res) => { res.end('Hello'); });

const keepAliveAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: KEEP_ALIVE,
});

server.listen(0, () => {
  for (let i = 0; i < N; ++i) {
    http.get(
      { port: server.address().port, agent: keepAliveAgent },
      (res) => { res.resume(); }
    );
  }
});

function checkOnExit() {
  assert.deepStrictEqual(destroyedIds, createdIds);
  // Each HTTP request results in two createdIds and two destroyedIds.
  assert.strictEqual(createdIds.size, N * 2);
}

// Ordinary exit.
process.on('exit', checkOnExit);

// Test runner tools/test.py will send SIGTERM if timeout.
// Signals aren't received in workers, but that's OK. The test will still fail
// if it times out. It just won't provide the additional information that this
// handler does.
process.on('SIGTERM', () => {
  console.error('Received SIGTERM. (Timed out?)');
  checkOnExit();
  process.exit(1);
});
