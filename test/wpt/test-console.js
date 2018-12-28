'use strict';

const common = require('../common');
common.requireFlags('--expose-internals');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('console');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['console']);

runner.runJsTests();
