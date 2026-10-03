'use strict';

const { WPTRunner } = require('../common/wpt');

// Standalone worker threads share a LockManager; managed groups have their own process.
const runner = new WPTRunner('web-locks', { concurrency: 1 });

runner.pretendGlobalThisAs('Window');
runner.runJsTests();
