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
const net = require('net');

const buf = Buffer.alloc(10 * 1024 * 1024, 0x62);

const errs = [];
let clientSocket;
let serverSocket;

function ready() {
  if (clientSocket && serverSocket) {
    clientSocket.destroy();
    serverSocket.write(buf);
  }
}

const srv = net.createServer(function onConnection(conn) {
  conn.on('error', function(err) {
    errs.push(err);
    if (errs.length > 1 && errs[0] === errs[1])
      assert.fail('Should not emit the same error twice');
  });
  conn.on('close', function() {
    srv.unref();
  });
  serverSocket = conn;
  ready();
}).listen(0, function() {
  const client = net.connect({ port: this.address().port });

  client.on('connect', function() {
    clientSocket = client;
    ready();
  });
});

process.on('exit', function() {
  console.log(errs);
  assert.strictEqual(errs.length, 1);
});
