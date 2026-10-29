'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const { async_id_symbol } = require('internal/async_hooks').symbols;
const async_hooks = require('async_hooks');
const http = require('http');

// Regression test for https://github.com/nodejs/node/issues/19859
// Checks that an http.Agent emits a destroy for the old asyncId before calling
// asyncReset()s when reusing a socket handle. The setup is nearly identical to
// parallel/test-async-hooks-http-agent (which focuses on the assertion that
// a fresh asyncId is assigned to the net.Socket instance).

const destroyedIds = new Set();
async_hooks.createHook({
  destroy: common.mustCallAtLeast((asyncId) => {
    destroyedIds.add(asyncId);
  }, 1)
}).enable();

// Make sure a single socket is transparently reused for 2 requests.
const agent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: Infinity,
  maxSockets: 1
});

const server = http.createServer(common.mustCall((req, res) => {
  req.once('data', common.mustCallAtLeast(() => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write('foo');
  }));
  req.on('end', common.mustCall(() => {
    res.end('bar');
  }));
}, 2)).listen(0, common.mustCall(() => {
  const port = server.address().port;
  const payload = 'hello world';

  // First request. This is useless except for adding a socket to the
  // agent’s pool for reuse.
  const r1 = http.request({
    agent, port, method: 'POST'
  }, common.mustCall((res) => {
    // Remember which socket we used.
    const socket = res.socket;
    const asyncIdAtFirstRequest = socket[async_id_symbol];
    assert.ok(asyncIdAtFirstRequest > 0, `${asyncIdAtFirstRequest} > 0`);
    // Check that request and response share their socket.
    assert.strictEqual(r1.socket, socket);

    res.on('data', common.mustCallAtLeast());
    res.on('end', common.mustCall(() => {
      // setImmediate() to give the agent time to register the freed socket.
      setImmediate(common.mustCall(() => {
        // The socket is free for reuse now.
        assert.strictEqual(socket[async_id_symbol], -1);

        // second request:
        const r2 = http.request({
          agent, port, method: 'POST'
        }, common.mustCall((res) => {
          assert.ok(destroyedIds.has(asyncIdAtFirstRequest));

          // Empty payload, to hit the “right” code path.
          r2.end('');

          res.on('data', common.mustCallAtLeast());
          res.on('end', common.mustCall(() => {
            // Clean up to let the event loop stop.
            server.close();
            agent.destroy();
          }));
        }));

        // Schedule a payload to be written immediately, but do not end the
        // request just yet.
        r2.write(payload);
      }));
    }));
  }));
  r1.end(payload);
}));
