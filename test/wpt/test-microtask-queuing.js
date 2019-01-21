'use strict';

// Flags: --expose-internals

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('html/webappapis/microtask-queuing');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['queueMicrotask']);

runner.runJsTests();
