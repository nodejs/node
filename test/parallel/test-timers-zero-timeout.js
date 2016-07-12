'use strict';
const common = require('../common');
const assert = require('assert');

// https://github.com/joyent/node/issues/2079 - zero timeout drops extra args
{
  setTimeout(common.mustCall(f), 0, 'foo', 'bar', 'baz');
  setTimeout(function() {}, 0);

  function f(a, b, c) {
    assert.strictEqual(a, 'foo');
    assert.strictEqual(b, 'bar');
    assert.strictEqual(c, 'baz');
  }
}

{
  let ncalled = 0;

  const iv = setInterval(f, 0, 'foo', 'bar', 'baz');

  function f(a, b, c) {
    assert.strictEqual(a, 'foo');
    assert.strictEqual(b, 'bar');
    assert.strictEqual(c, 'baz');
    if (++ncalled == 3) clearTimeout(iv);
  }

  process.on('exit', function() {
    assert.strictEqual(ncalled, 3);
  });
}
