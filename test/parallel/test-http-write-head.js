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
      assert.notStrictEqual(response.rawHeaders.indexOf('Test'), -1);
      s.close();
    }));
    response.resume();
  }));
}
