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
// I hate HTTP. One way of terminating an HTTP response is to not send
// a content-length header, not send a transfer-encoding: chunked header,
// and simply terminate the TCP connection. That is identity
// transfer-encoding.
//
// This test is to be sure that the https client is handling this case
// correctly.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fs = require('fs');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
};


const server = tls.Server(options, function(socket) {
  console.log('2) Server got request');
  socket.write('HTTP/1.1 200 OK\r\n' +
               'Date: Tue, 15 Feb 2011 22:14:54 GMT\r\n' +
               'Expires: -1\r\n' +
               'Cache-Control: private, max-age=0\r\n' +
               'Set-Cookie: xyz\r\n' +
               'Set-Cookie: abc\r\n' +
               'Server: gws\r\n' +
               'X-XSS-Protection: 1; mode=block\r\n' +
               'Connection: close\r\n' +
               '\r\n');

  socket.write('hello world\n');

  setTimeout(function() {
    socket.end('hello world\n');
    console.log('4) Server finished response');
  }, 100);
});

server.listen(0, common.mustCall(function() {
  console.log('1) Making Request');
  https.get({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function(res) {
    let bodyBuffer = '';

    server.close();
    console.log('3) Client got response headers.');

    assert.strictEqual('gws', res.headers.server);

    res.setEncoding('utf8');
    res.on('data', function(s) {
      bodyBuffer += s;
    });

    res.on('end', common.mustCall(function() {
      console.log('5) Client got "end" event.');
      assert.strictEqual('hello world\nhello world\n', bodyBuffer);
    }));
  }));
}));
