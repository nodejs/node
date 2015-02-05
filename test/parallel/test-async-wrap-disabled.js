var common = require('../common');
var assert = require('assert');

var asyncWrap = process.binding('async_wrap');

var asyncHooksObject = {};
var kEnabled = 1;
asyncWrap.setupHooks(asyncHooksObject, init, before, after);
asyncHooksObject[kEnabled] = 0;

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
    'callback'
  ]);
});
