'use strict';
const common = require('../common');
const timeout = setTimeout(common.fail, 2 ** 30); // Keep event loop open.
const interval = setInterval(common.mustCall(() => {
  // Use clearTimeout() instead of clearInterval().
  // The interval should still be cleared.
  clearTimeout(interval);
  setImmediate(() => { clearTimeout(timeout); });
}), 1).unref();
