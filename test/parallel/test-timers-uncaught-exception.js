'use strict';
const common = require('../common');
const assert = require('assert');
const errorMsg = 'BAM!';

// the first timer throws...
setTimeout(common.mustCall(function() {
  throw new Error(errorMsg);
}), 100);

// ...but the second one should still run
setTimeout(common.mustCall(function() {}), 100);

function uncaughtException(err) {
  assert.strictEqual(err.message, errorMsg);
}

process.on('uncaughtException', common.mustCall(uncaughtException));

process.on('exit', function() {
  process.removeListener('uncaughtException', uncaughtException);
});
