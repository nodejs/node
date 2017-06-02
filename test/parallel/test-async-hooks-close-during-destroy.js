'use strict';
// Test that async ids that are added to the destroy queue while running a
// `destroy` callback are handled correctly.

const common = require('../common');
const assert = require('assert');
const net = require('net');
const async_hooks = require('async_hooks');

const initCalls = new Set();
let destroyTcpWrapCallCount = 0;
let srv2;

async_hooks.createHook({
  init: common.mustCallAtLeast((id, provider, triggerId) => {
    if (provider === 'TCPWRAP')
      initCalls.add(id);
  }, 2),
  destroy: common.mustCallAtLeast((id) => {
    if (!initCalls.has(id)) return;

    switch (destroyTcpWrapCallCount++) {
      case 0:
        // Trigger the second `destroy` call.
        srv2.close();
        break;
      case 2:
        assert.fail('More than 2 destroy() invocations');
        break;
    }
  }, 2)
}).enable();

// Create a server to trigger the first `destroy` callback.
net.createServer().listen(0).close();
srv2 = net.createServer().listen(0);

process.on('exit', () => assert.strictEqual(destroyTcpWrapCallCount, 2));
