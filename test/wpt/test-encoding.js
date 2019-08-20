'use strict';

// Flags: --expose-internals

require('../common');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['TextDecoder', 'TextEncoder']);

runner.runJsTests();
