'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

// Verify that ServerResponse.writeHead() works as setHeader.
// Issue 5036 on github.

var s = http.createServer(function(req, res) {
  res.setHeader('test', '1');

  // toLowerCase() is used on the name argument, so it must be a string.
  var threw = false;
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
    assert.equal(e.message, '"value" required in setHeader("foo", value)');
    threw = true;
  }
  assert.ok(threw, 'Undefined value should throw');

  res.writeHead(200, { Test: '2' });
  res.end();
});

s.listen(0, runTest);

function runTest() {
  http.get({ port: this.address().port }, function(response) {
    response.on('end', function() {
      assert.equal(response.headers['test'], '2');
      assert.notStrictEqual(response.rawHeaders.indexOf('Test'), -1);
      s.close();
    });
    response.resume();
  });
}
