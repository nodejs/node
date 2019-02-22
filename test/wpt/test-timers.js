'use strict';

// Flags: --expose-internals

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('html/webappapis/timers');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, [
  'setInterval',
  'clearInterval',
  'setTimeout',
  'clearTimeout'
]);

runner.runJsTests();
