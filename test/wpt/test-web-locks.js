'use strict';

const { WPTRunner } = require('../common/wpt');

// Run serially to avoid cross-test interference on the shared LockManager.
const runner = new WPTRunner('web-locks', { concurrency: 1 });

runner.pretendGlobalThisAs('Window');
runner.runJsTests();
