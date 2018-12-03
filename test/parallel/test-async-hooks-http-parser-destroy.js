'use strict';
const common = require('../common');
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
  init: common.mustCallAtLeast((asyncId, type) => {
    if (type === 'HTTPPARSER') {
      createdIds.push(asyncId);
    }
  }, N),
  destroy: (asyncId) => {
    destroyedIds.push(asyncId);
  }
}).enable();

const server = http.createServer(function(req, res) {
  res.end('Hello');
});

const keepAliveAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: KEEP_ALIVE,
});

const countdown = new Countdown(N, () => {
  server.close(() => {
    // Give the server sockets time to close (which will also free their
    // associated parser objects) after the server has been closed.
    setTimeout(() => {
      createdIds.forEach((createdAsyncId) => {
        assert.ok(destroyedIds.indexOf(createdAsyncId) >= 0);
      });
    }, KEEP_ALIVE * 2);
  });
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
