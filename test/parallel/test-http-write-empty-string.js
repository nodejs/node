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

const server = http.createServer(function(request, response) {
  console.log(`responding to ${request.url}`);

  response.writeHead(200, { 'Content-Type': 'text/plain' });
  response.write('1\n');
  response.write('');
  response.write('2\n');
  response.write('');
  response.end('3\n');

  this.close();
});

server.listen(0, common.mustCall(function() {
  http.get({ port: this.address().port }, common.mustCall(function(res) {
    let response = '';

    assert.strictEqual(200, res.statusCode);
    res.setEncoding('ascii');
    res.on('data', function(chunk) {
      response += chunk;
    });
    res.on('end', common.mustCall(function() {
      assert.strictEqual('1\n2\n3\n', response);
    }));
  }));
}));
