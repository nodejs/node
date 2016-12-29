'use strict';
const common = require('../common');
const assert = require('assert');

const errorMsg = 'BAM!';

// the first timer throws...
setTimeout(common.mustCall(function() {
  throw new Error(errorMsg);
}), 1);

// ...but the second one should still run
setTimeout(common.mustCall(function() {}), 1);

function uncaughtException(err) {
  assert.strictEqual(err.message, errorMsg);
}

process.on('uncaughtException', common.mustCall(uncaughtException));
