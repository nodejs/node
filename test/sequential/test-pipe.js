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

const webPort = common.PORT;
const tcpPort = webPort + 1;
const bufferSize = 5 * 1024 * 1024;

let listenCount = 0;
let gotThanks = false;
let tcpLengthSeen = 0;


/*
 * 5MB of random buffer.
 */
const buffer = Buffer.allocUnsafe(bufferSize);
for (let i = 0; i < buffer.length; i++) {
  buffer[i] = parseInt(Math.random() * 10000) % 256;
}


const web = http.Server(common.mustCall((req, res) => {
  web.close();

  const socket = net.Stream();
  socket.connect(tcpPort);

  socket.on('connect', common.mustCall());

  req.pipe(socket);

  req.on('end', common.mustCall(() => {
    res.writeHead(200);
    res.write('thanks');
    res.end();
  }));

  req.connection.on('error', (e) => {
    assert.ifError(e);
  });
}));

web.listen(webPort, startClient);


const tcp = net.Server(common.mustCall((s) => {
  tcp.close();

  let i = 0;

  s.on('data', (d) => {
    tcpLengthSeen += d.length;
    for (let j = 0; j < d.length; j++) {
      assert.strictEqual(d[j], buffer[i]);
      i++;
    }
  });

  s.on('end', common.mustCall(() => {
    s.end();
  }));

  s.on('error', (e) => {
    assert.ifError(e);
  });
}));

tcp.listen(tcpPort, startClient);

function startClient() {
  listenCount++;
  if (listenCount < 2) return;

  const req = http.request({
    port: common.PORT,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall((res) => {
    res.setEncoding('utf8');
    res.on('data', common.mustCall((string) => {
      assert.strictEqual(string, 'thanks');
      gotThanks = true;
    }));
  }));
  req.write(buffer);
  req.end();
}

process.on('exit', () => {
  assert.ok(gotThanks);
  assert.strictEqual(tcpLengthSeen, bufferSize);
});
