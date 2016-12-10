'use strict';
const common = require('../common');
var assert = require('assert');

process.nextTick(common.mustCall(function() {
  process.nextTick(common.mustCall(function() {
    process.nextTick(common.mustCall(function() {}));
  }));
}));

setTimeout(common.mustCall(function() {
  process.nextTick(common.mustCall(function() {}));
}), 50);

process.nextTick(common.mustCall(function() {}));

var obj = {};

process.nextTick(function(a, b) {
  assert.equal(a, 42);
  assert.equal(b, obj);
}, 42, obj);

process.on('exit', function() {
  process.nextTick(common.fail);
});
