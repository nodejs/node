'use strict';
const common = require('../common');

// This test ensures that if an Immediate callback clears subsequent
// immediates we don't get stuck in an infinite loop.
//
// If the process does get stuck, it will be timed out by the test
// runner.
//
// Ref: https://github.com/nodejs/node/issues/9756

setImmediate(common.mustCall(function() {
  clearImmediate(i2);
  clearImmediate(i3);
}));

const i2 = setImmediate(function() {
  common.fail('immediate callback should not have fired');
});

const i3 = setImmediate(function() {
  common.fail('immediate callback should not have fired');
});
