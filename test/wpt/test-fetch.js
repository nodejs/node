'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('fetch/api');

runner.pretendGlobalThisAs('Window');
// The window harness does not define GLOBAL.isWorker(). The idlharness
// setup calls it when isWindow() is false.
runner.setInitScript(
  'globalThis.GLOBAL.isWorker = function() { return false; };',
);
runner.runJsTests();
