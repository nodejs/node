'use strict';

const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

// Set a script that will be executed in the worker before running the tests.
runner.pretendGlobalThisAs('Window');

runner.runJsTests();
