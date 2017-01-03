// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
    if (++ncalled === 3) clearTimeout(iv);
  }

  process.on('exit', function() {
    assert.strictEqual(ncalled, 3);
  });
}
