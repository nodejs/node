'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('streams');

// Set a script that will be executed in the worker before running the tests.
runner.setInitScript(`
  // Simulate globalThis postMessage for enqueue-with-detached-buffer.window.js
  function postMessage(value, origin, transferList) {
    const mc = new MessageChannel();
    mc.port1.postMessage(value, transferList);
    mc.port2.close();
  }
`);

runner.runJsTests();
