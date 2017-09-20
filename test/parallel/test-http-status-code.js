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
require('../common');
const assert = require('assert');
const http = require('http');

// Simple test of Node's HTTP ServerResponse.statusCode
// ServerResponse.prototype.statusCode

let testsComplete = 0;
const tests = [200, 202, 300, 404, 451, 500];
let testIdx = 0;

const s = http.createServer(function(req, res) {
  const t = tests[testIdx];
  res.writeHead(t, { 'Content-Type': 'text/plain' });
  console.log(`--\nserver: statusCode after writeHead: ${res.statusCode}`);
  assert.strictEqual(res.statusCode, t);
  res.end('hello world\n');
});

s.listen(0, nextTest);


function nextTest() {
  if (testIdx + 1 === tests.length) {
    return s.close();
  }
  const test = tests[testIdx];

  http.get({ port: s.address().port }, function(response) {
    console.log(`client: expected status: ${test}`);
    console.log(`client: statusCode: ${response.statusCode}`);
    assert.strictEqual(response.statusCode, test);
    response.on('end', function() {
      testsComplete++;
      testIdx += 1;
      nextTest();
    });
    response.resume();
  });
}


process.on('exit', function() {
  assert.strictEqual(5, testsComplete);
});
