'use strict';
require('../common');
const assert = require('assert');

let called = 0;
let closed = 0;

const timeout = setTimeout(function() {
  called++;
}, 10);
timeout.unref();

// Wrap `close` method to check if the handle was closed
const close = timeout._handle.close;
timeout._handle.close = function() {
  closed++;
  return close.apply(this, arguments);
};

// Just to keep process alive and let previous timer's handle die
setTimeout(function() {
}, 50);

process.on('exit', function() {
  assert.strictEqual(called, 1);
  assert.strictEqual(closed, 1);
});
