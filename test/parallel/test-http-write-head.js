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
const http = require('http');

// Verify that ServerResponse.writeHead() works as setHeader.
// Issue 5036 on github.

const s = http.createServer(common.mustCall((req, res) => {
  res.setHeader('test', '1');

  // toLowerCase() is used on the name argument, so it must be a string.
  let threw = false;
  try {
    res.setHeader(0xf00, 'bar');
  } catch (e) {
    assert.ok(e instanceof TypeError);
    threw = true;
  }
  assert.ok(threw, 'Non-string names should throw');

  // undefined value should throw, via 979d0ca8
  threw = false;
  try {
    res.setHeader('foo', undefined);
  } catch (e) {
    assert.ok(e instanceof Error);
    assert.strictEqual(e.message,
                       '"value" required in setHeader("foo", value)');
    threw = true;
  }
  assert.ok(threw, 'Undefined value should throw');

  res.writeHead(200, { Test: '2' });

  assert.throws(() => {
    res.writeHead(100, {});
  }, /^Error: Can't render headers after they are sent to the client$/);

  res.end();
}));

s.listen(0, common.mustCall(runTest));

function runTest() {
  http.get({ port: this.address().port }, common.mustCall((response) => {
    response.on('end', common.mustCall(() => {
      assert.strictEqual(response.headers['test'], '2');
      assert(response.rawHeaders.includes('Test'));
      s.close();
    }));
    response.resume();
  }));
}
