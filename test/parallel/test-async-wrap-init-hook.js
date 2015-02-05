var common = require('../common');
var assert = require('assert');

var asyncWrap = process.binding('async_wrap');

var asyncHooksObject = {};
var kCallInitHook = 0;
asyncWrap.setupHooks(asyncHooksObject, init, before, after);
asyncHooksObject[kCallInitHook] = 1;

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
    'init Timer', 'before Timer', 'callback', 'after Timer'
  ]);
});
