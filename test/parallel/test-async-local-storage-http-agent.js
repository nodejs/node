'use strict';
const common = require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const http = require('node:http');

// Similar as test-async-hooks-http-agent added via
// https://github.com/nodejs/node/issues/13325 but verifies
// AsyncLocalStorage functionality instead async_hooks

const cls = new AsyncLocalStorage();

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
  cls.run('first', common.mustCall(() => {
    assert.strictEqual(cls.getStore(), 'first');
    const r1 = http.request({
      agent, port, method: 'POST'
    }, common.mustCall((res) => {
      assert.strictEqual(cls.getStore(), 'first');
      res.on('data', common.mustCallAtLeast(() => {
        assert.strictEqual(cls.getStore(), 'first');
      }));
      res.on('end', common.mustCall(() => {
        assert.strictEqual(cls.getStore(), 'first');
        // setImmediate() to give the agent time to register the freed socket.
        setImmediate(common.mustCall(() => {
          assert.strictEqual(cls.getStore(), 'first');

          cls.run('second', common.mustCall(() => {
            // Second request. To re-create the exact conditions from the
            // referenced issue, we use a POST request without chunked encoding
            // (hence the Content-Length header) and call .end() after the
            // response header has already been received.
            const r2 = http.request({
              agent, port, method: 'POST', headers: {
                'Content-Length': payload.length
              }
            }, common.mustCall((res) => {
              assert.strictEqual(cls.getStore(), 'second');
              // Empty payload, to hit the “right” code path.
              r2.end('');

              res.on('data', common.mustCallAtLeast(() => {
                assert.strictEqual(cls.getStore(), 'second');
              }));
              res.on('end', common.mustCall(() => {
                assert.strictEqual(cls.getStore(), 'second');
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
    }));
    r1.end(payload);
  }));
}));
