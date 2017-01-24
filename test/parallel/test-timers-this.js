'use strict';
const common = require('../common');
const assert = require('assert');

const immediateHandler = setImmediate(common.mustCall(function() {
  assert.strictEqual(this, immediateHandler);
}));

const immediateArgsHandler = setImmediate(common.mustCall(function() {
  assert.strictEqual(this, immediateArgsHandler);
}), 'args ...');

const intervalHandler = setInterval(common.mustCall(function() {
  clearInterval(intervalHandler);
  assert.strictEqual(this, intervalHandler);
}), 1);

const intervalArgsHandler = setInterval(common.mustCall(function() {
  clearInterval(intervalArgsHandler);
  assert.strictEqual(this, intervalArgsHandler);
}), 1, 'args ...');

const timeoutHandler = setTimeout(common.mustCall(function() {
  assert.strictEqual(this, timeoutHandler);
}), 1);

const timeoutArgsHandler = setTimeout(common.mustCall(function() {
  assert.strictEqual(this, timeoutArgsHandler);
}), 1, 'args ...');
