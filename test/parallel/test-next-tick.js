'use strict';
const common = require('../common');
const assert = require('assert');

process.nextTick(common.mustCall(function() {
  process.nextTick(common.mustCall(function() {
    process.nextTick(common.mustCall());
  }));
}));

setTimeout(common.mustCall(function() {
  process.nextTick(common.mustCall());
}), 50);

process.nextTick(common.mustCall());

const obj = {};

process.nextTick(function(a, b) {
  assert.strictEqual(a, 42);
  assert.strictEqual(b, obj);
}, 42, obj);

process.on('exit', function() {
  process.nextTick(common.mustNotCall());
});
