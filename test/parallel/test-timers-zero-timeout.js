'use strict';
const common = require('../common');
const assert = require('assert');

// https://github.com/joyent/node/issues/2079 - zero timeout drops extra args
{
  setTimeout(common.mustCall(f), 0, 'foo', 'bar', 'baz');
  setTimeout(() => {}, 0);

  function f(a, b, c) {
    assert.strictEqual(a, 'foo');
    assert.strictEqual(b, 'bar');
    assert.strictEqual(c, 'baz');
  }
}

{
  let ncalled = 3;

  const f = common.mustCall((a, b, c) => {
    assert.strictEqual(a, 'foo');
    assert.strictEqual(b, 'bar');
    assert.strictEqual(c, 'baz');
    if (--ncalled === 0) clearTimeout(iv);
  }, ncalled);

  const iv = setInterval(f, 0, 'foo', 'bar', 'baz');
}
