'use strict';
const common = require('../common');
const assert = require('assert');

process.nextTick(common.mustCall(function() {
  process.nextTick(common.mustCall(function() {
    process.nextTick(common.mustCall(function() {}));
  }));
}));

setTimeout(common.mustCall(function() {
  process.nextTick(common.mustCall(function() {}));
}), 50);

process.nextTick(common.mustCall(function() {}));

const obj = {};

process.nextTick(function(a, b) {
  assert.strictEqual(a, 42);
  assert.strictEqual(b, obj);
}, 42, obj);

process.on('exit', function() {
  process.nextTick(common.mustNotCall());
});
