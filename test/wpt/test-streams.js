'use strict';

const { WPTRunner } = require('../common/wpt');

// Runs each spec in its own process; this suite has crashed the runner in CI.
const runner = new WPTRunner('streams', { backend: 'process' });

// Set a script that will be executed in the worker before running the tests.
runner.pretendGlobalThisAs('Window');

runner.runJsTests();
