'use strict';
// Test that async ids that are added to the destroy queue while running a
// `destroy` callback are handled correctly.

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const initCalls = new Set();
let destroyResCallCount = 0;
let res2;

async_hooks.createHook({
  init: common.mustCallAtLeast((id, provider, triggerAsyncId) => {
    if (provider === 'foobar')
      initCalls.add(id);
  }, 2),
  destroy: common.mustCallAtLeast((id) => {
    if (!initCalls.has(id)) return;

    switch (destroyResCallCount++) {
      case 0:
        // Trigger the second `destroy` call.
        res2.emitDestroy();
        break;
      case 2:
        assert.fail('More than 2 destroy() invocations');
        break;
    }
  }, 2)
}).enable();

const res1 = new async_hooks.AsyncResource('foobar');
res2 = new async_hooks.AsyncResource('foobar');
res1.emitDestroy();

process.on('exit', () => assert.strictEqual(destroyResCallCount, 2));
