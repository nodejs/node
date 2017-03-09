'use strict';
require('../common');
const assert = require('assert');

const order = [];
process.nextTick(function() {
  setTimeout(function() {
    order.push('setTimeout');
  }, 0);

  process.nextTick(function() {
    order.push('nextTick');
  });
});

process.on('exit', function() {
  assert.deepStrictEqual(order, ['nextTick', 'setTimeout']);
});
