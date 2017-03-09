'use strict';
require('../common');
const assert = require('assert');

// https://github.com/joyent/node/issues/2079 - zero timeout drops extra args
(function() {
  let ncalled = 0;

  setTimeout(f, 0, 'foo', 'bar', 'baz');
  setTimeout(function() {}, 0);

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
  let ncalled = 0;

  const iv = setInterval(f, 0, 'foo', 'bar', 'baz');

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
