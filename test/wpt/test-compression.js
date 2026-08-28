'use strict';

const { WPTRunner } = require('../common/wpt');

// Runs each spec in its own process; this suite has crashed the runner in CI.
const runner = new WPTRunner('compression', { backend: 'process' });

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
