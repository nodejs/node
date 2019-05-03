'use strict';
require('../common');
const Countdown = require('../common/countdown');
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
  }
}).enable();

const server = http.createServer((req, res) => {
  res.end('Hello');
});

const keepAliveAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: KEEP_ALIVE,
});

const countdown = new Countdown(N, () => {
  server.close();
});

server.listen(0, function() {
  for (let i = 0; i < N; ++i) {
    (function makeRequest() {
      http.get({
        port: server.address().port,
        agent: keepAliveAgent
      }, function(res) {
        countdown.dec();
        res.resume();
      });
    })();
  }
});

function checkOnExit() {
  assert.deepStrictEqual(destroyedIds.sort(), createdIds.sort());
  // There should be at least one ID for each request.
  assert.ok(createdIds.length >= N, `${createdIds.length} < ${N}`);
}

// Ordinary exit.
process.on('exit', checkOnExit);
