'use strict';
var common = require('../common');
var assert = require('assert');

var order = [];
process.nextTick(function() {
  setTimeout(function() {
    order.push('setTimeout');
  }, 0);

  process.nextTick(function() {
    order.push('nextTick');
  });
});

process.on('exit', function() {
  assert.deepEqual(order, ['nextTick', 'setTimeout']);
});
