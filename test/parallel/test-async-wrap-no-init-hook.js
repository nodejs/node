var common = require('../common');
var assert = require('assert');

var asyncWrap = process.binding('async_wrap');

var asyncHooksObject = {};
asyncWrap.setupHooks(asyncHooksObject, init, before, after);

var order = [];

function init() {
  order.push('init ' + this.constructor.name);
}

function before() {
  order.push('before ' + this.constructor.name);
}

function after() {
  order.push('after ' + this.constructor.name);
}

setTimeout(function () {
  order.push('callback');
});

process.once('exit', function () {
  assert.deepEqual(order, [
    'before Timer', 'callback', 'after Timer'
  ]);
});
