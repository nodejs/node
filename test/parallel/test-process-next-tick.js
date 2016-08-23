'use strict';
const common = require('../common');
const assert = require('assert');
const N = 2;

function cb() {
  throw new Error();
}

for (var i = 0; i < N; ++i) {
  process.nextTick(common.mustCall(cb));
}

process.on('uncaughtException', common.mustCall(function(err) {
  // assert the async error should have this file line
  assert(/test\/parallel\/test-aprocess-next-tick.js/.test(err.stack));
}, N));

process.on('exit', function() {
  process.removeAllListeners('uncaughtException');
});
