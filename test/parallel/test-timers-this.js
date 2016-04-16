'use strict';
require('../common');
var assert = require('assert');

let immediateThis, intervalThis, timeoutThis;
let immediateArgsThis, intervalArgsThis, timeoutArgsThis;

var immediateHandler = setImmediate(function() {
  immediateThis = this;
});

var immediateArgsHandler = setImmediate(function() {
  immediateArgsThis = this;
}, 'args ...');

var intervalHandler = setInterval(function() {
  clearInterval(intervalHandler);

  intervalThis = this;
});

var intervalArgsHandler = setInterval(function() {
  clearInterval(intervalArgsHandler);

  intervalArgsThis = this;
}, 0, 'args ...');

var timeoutHandler = setTimeout(function() {
  timeoutThis = this;
});

var timeoutArgsHandler = setTimeout(function() {
  timeoutArgsThis = this;
}, 0, 'args ...');

process.once('exit', function() {
  assert.strictEqual(immediateThis, immediateHandler);
  assert.strictEqual(immediateArgsThis, immediateArgsHandler);

  assert.strictEqual(intervalThis, intervalHandler);
  assert.strictEqual(intervalArgsThis, intervalArgsHandler);

  assert.strictEqual(timeoutThis, timeoutHandler);
  assert.strictEqual(timeoutArgsThis, timeoutArgsHandler);
});
