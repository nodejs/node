'use strict';
var common = require('../common');
var assert = require('assert');

// https://github.com/joyent/node/issues/2079 - zero timeout drops extra args
(function() {
  var ncalled = 0;

  setTimeout(f, 0, 'foo', 'bar', 'baz');
  var timer = setTimeout(function() {}, 0);

  function f(a, b, c) {
    assert.equal(a, 'foo');
    assert.equal(b, 'bar');
    assert.equal(c, 'baz');
    ncalled++;
  }

  process.on('exit', function() {
    assert.equal(ncalled, 1);
  });
})();

(function() {
  var ncalled = 0;

  var iv = setInterval(f, 0, 'foo', 'bar', 'baz');

  function f(a, b, c) {
    assert.equal(a, 'foo');
    assert.equal(b, 'bar');
    assert.equal(c, 'baz');
    if (++ncalled == 3) clearTimeout(iv);
  }

  process.on('exit', function() {
    assert.equal(ncalled, 3);
  });
})();
