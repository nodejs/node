'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('streams');

// Set a script that will be executed in the worker before running the tests.
runner.setInitScript(`
  // Simulate global postMessage for enqueue-with-detached-buffer.window.js
  function postMessage(value, origin, transferList) {
    const mc = new MessageChannel();
    mc.port1.postMessage(value, transferList);
    mc.port2.close();
  }

  // TODO(@jasnell): This is a bit of a hack to get the idl harness test
  // working. Later we should investigate a better approach.
  // See: https://github.com/nodejs/node/pull/39062#discussion_r659383373
  Object.defineProperties(global, {
    DedicatedWorkerGlobalScope: {
      get() {
        // Pretend that we're a DedicatedWorker, but *only* for the
        // IDL harness. For everything else, keep the JavaScript shell
        // environment.
        if (new Error().stack.includes('idlharness.js'))
          return global.constructor;
        else
          return function() {};
      }
    }
  });
`);

runner.runJsTests();
