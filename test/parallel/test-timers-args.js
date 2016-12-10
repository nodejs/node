'use strict';
require('../common');
var assert = require('assert');

function range(n) {
  return 'x'.repeat(n + 1).split('').map(function(_, i) { return i; });
}

function timeout(nargs) {
  var args = range(nargs);
  setTimeout.apply(null, [callback, 1].concat(args));

  function callback() {
    assert.deepStrictEqual([].slice.call(arguments), args);
    if (nargs < 128) timeout(nargs + 1);
  }
}

function interval(nargs) {
  var args = range(nargs);
  var timer = setTimeout.apply(null, [callback, 1].concat(args));

  function callback() {
    clearInterval(timer);
    assert.deepStrictEqual([].slice.call(arguments), args);
    if (nargs < 128) interval(nargs + 1);
  }
}

timeout(0);
interval(0);
