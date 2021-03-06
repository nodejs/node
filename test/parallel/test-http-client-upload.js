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

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual(req.method, 'POST');
  req.setEncoding('utf8');

  let sent_body = '';

  req.on('data', function(chunk) {
    console.log(`server got: ${JSON.stringify(chunk)}`);
    sent_body += chunk;
  });

  req.on('end', common.mustCall(function() {
    assert.strictEqual(sent_body, '1\n2\n3\n');
    console.log('request complete from server');
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write('hello\n');
    res.end();
  }));
}));
server.listen(0);

server.on('listening', common.mustCall(function() {
  const req = http.request({
    port: this.address().port,
    method: 'POST',
    path: '/'
  }, common.mustCall(function(res) {
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      console.log(chunk);
    });
    res.on('end', common.mustCall(function() {
      server.close();
    }));
  }));

  req.write('1\n');
  req.write('2\n');
  req.write('3\n');
  req.end();
}));
