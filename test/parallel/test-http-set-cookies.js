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
const Countdown = require('../common/countdown');

const countdown = new Countdown(2, () => server.close());
const server = http.createServer(function(req, res) {
  if (req.url === '/one') {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['content-type', 'text/plain']]);
    res.end('one\n');
  } else {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['set-cookie', 'B'],
                        ['content-type', 'text/plain']]);
    res.end('two\n');
  }
});
server.listen(0);

server.on('listening', function() {
  //
  // one set-cookie header
  //
  http.get({ port: this.address().port, path: '/one' }, function(res) {
    // set-cookie headers are always return in an array.
    // even if there is only one.
    assert.deepStrictEqual(res.headers['set-cookie'], ['A']);
    assert.strictEqual(res.headers['content-type'], 'text/plain');

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      countdown.dec();
    });
  });

  // Two set-cookie headers

  http.get({ port: this.address().port, path: '/two' }, function(res) {
    assert.deepStrictEqual(res.headers['set-cookie'], ['A', 'B']);
    assert.strictEqual(res.headers['content-type'], 'text/plain');

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      countdown.dec();
    });
  });

});
