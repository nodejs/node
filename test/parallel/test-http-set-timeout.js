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
const net = require('net');

const server = http.createServer(function(req, res) {
  console.log('got request. setting 500ms timeout');
  const socket = req.connection.setTimeout(500);
  assert.ok(socket instanceof net.Socket);
  req.connection.on('timeout', common.mustCall(function() {
    req.connection.destroy();
    console.error('TIMEOUT');
    server.close();
  }));
});

server.listen(0, function() {
  console.log(`Server running at http://127.0.0.1:${this.address().port}/`);

  const request = http.get({ port: this.address().port, path: '/' });
  request.on('error', common.mustCall(function() {
    console.log('HTTP REQUEST COMPLETE (this is good)');
  }));
  request.end();
});
