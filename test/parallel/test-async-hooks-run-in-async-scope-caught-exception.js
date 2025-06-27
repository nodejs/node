'use strict';

require('../common');
const { AsyncResource } = require('async_hooks');

try {
  new AsyncResource('foo').runInAsyncScope(() => { throw new Error('bar'); });
} catch {
  // Continue regardless of error.
}
// Should abort (fail the case) if async id is not matching.
