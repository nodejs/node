'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('web-locks');

// Needed to access to Lock and LockManager.
runner.setFlags(['--expose-internals']);

runner.setInitScript(`
  const { Lock, LockManager } = require('internal/worker/locks');
  global.isSecureContext = true;
  global.Lock = Lock;
  global.LockManager = LockManager;
  global.navigator = Object.create({
    locks: require('worker_threads').locks,
  });
`);

runner.runJsTests();
