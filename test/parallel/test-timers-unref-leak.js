'use strict';
require('../common');
var assert = require('assert');

var called = 0;
var closed = 0;

var timeout = setTimeout(function() {
  called++;
}, 10);
timeout.unref();

// Wrap `close` method to check if the handle was closed
var close = timeout._handle.close;
timeout._handle.close = function() {
  closed++;
  return close.apply(this, arguments);
};

// Just to keep process alive and let previous timer's handle die
setTimeout(function() {
}, 50);

process.on('exit', function() {
  assert.equal(called, 1);
  assert.equal(closed, 1);
});
