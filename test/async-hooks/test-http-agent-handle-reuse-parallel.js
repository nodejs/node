'use strict';
// Flags: --expose-internals
const common = require('../common');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const assert = require('assert');
const { async_id_symbol } = require('internal/async_hooks').symbols;
const http = require('http');

// Checks that the async resource used in init in case of a reused handle
// is not reused. Test is based on parallel\test-async-hooks-http-agent.js.

const hooks = initHooks();
hooks.enable();

const reqAsyncIds = [];
let socket;
let responses = 0;

// Make sure a single socket is transparently reused for 2 requests.
const agent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: Infinity,
  maxSockets: 1,
});

const verifyRequest = (idx) => (res) => {
  reqAsyncIds[idx] = res.socket[async_id_symbol];
  assert.ok(reqAsyncIds[idx] > 0, `${reqAsyncIds[idx]} > 0`);
  if (socket) {
    // Check that both requests share their socket.
    assert.strictEqual(res.socket, socket);
  } else {
    socket = res.socket;
  }

  res.on('data', common.mustCallAtLeast());
  res.on('end', common.mustCall(() => {
    if (++responses === 2) {
      // Clean up to let the event loop stop.
      server.close();
      agent.destroy();
    }
  }));
};

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

  // First request.
  const r1 = http.request({
    agent, port, method: 'POST',
  }, common.mustCall(verifyRequest(0)));
  r1.end(payload);

  // Second request. Sent in parallel with the first one.
  const r2 = http.request({
    agent, port, method: 'POST',
  }, common.mustCall(verifyRequest(1)));
  r2.end(payload);
}));


process.on('exit', onExit);

function onExit() {
  hooks.disable();
  hooks.sanityCheck();
  const activities = hooks.activities;

  // Verify both invocations
  const first = activities.filter((x) => x.uid === reqAsyncIds[0])[0];
  checkInvocations(first, { init: 1, destroy: 1 }, 'when process exits');

  const second = activities.filter((x) => x.uid === reqAsyncIds[1])[0];
  checkInvocations(second, { init: 1, destroy: 1 }, 'when process exits');

  // Verify reuse handle has been wrapped
  assert.strictEqual(first.type, second.type);
  assert.ok(first.handle !== second.handle, 'Resource reused');
  assert.ok(first.handle === second.handle.handle,
            'Resource not wrapped correctly');
}
