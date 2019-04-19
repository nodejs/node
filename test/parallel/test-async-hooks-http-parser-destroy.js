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

const createdIds = [];
const destroyedIds = [];
async_hooks.createHook({
  init: (asyncId, type) => {
    if (type === 'HTTPINCOMINGMESSAGE' || type === 'HTTPCLIENTREQUEST') {
      createdIds.push(asyncId);
    }
  },
  destroy: (asyncId) => {
    if (createdIds.includes(asyncId)) {
      destroyedIds.push(asyncId);
    }
    if (destroyedIds.length === 2 * N) {
      server.close();
    }
  }
}).enable();

const server = http.createServer((req, res) => {
  res.end('Hello');
});

const keepAliveAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: KEEP_ALIVE,
});

server.listen(0, function() {
  for (let i = 0; i < N; ++i) {
    (function makeRequest() {
      http.get({
        port: server.address().port,
        agent: keepAliveAgent
      }, function(res) {
        res.resume();
      });
    })();
  }
});

function checkOnExit() {
  assert.deepStrictEqual(destroyedIds.sort(), createdIds.sort());
  // There should be two IDs for each request.
  assert.strictEqual(createdIds.length, N * 2);
}

process.on('SIGTERM', () => {
  // Catching SIGTERM and calling `process.exit(1)`  so that the `exit` event
  // is triggered and the assertions are checked. This can be useful for
  // troubleshooting this test if it times out.
  process.exit(1);
});

// Ordinary exit.
process.on('exit', checkOnExit);
