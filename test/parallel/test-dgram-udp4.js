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
const dgram = require('dgram');
const message_to_send = 'A message to send';

const server = dgram.createSocket('udp4');
server.on('message', common.mustCall((msg, rinfo) => {
  assert.strictEqual(rinfo.address, common.localhostIPv4);
  assert.strictEqual(msg.toString(), message_to_send.toString());
  server.send(msg, 0, msg.length, rinfo.port, rinfo.address);
}));
server.on('listening', common.mustCall(() => {
  const client = dgram.createSocket('udp4');
  const port = server.address().port;
  client.on('message', common.mustCall((msg, rinfo) => {
    assert.strictEqual(rinfo.address, common.localhostIPv4);
    assert.strictEqual(rinfo.port, port);
    assert.strictEqual(msg.toString(), message_to_send.toString());
    client.close();
    server.close();
  }));
  client.send(message_to_send,
              0,
              message_to_send.length,
              port,
              'localhost');
  client.on('close', common.mustCall(() => {}));
}));
server.on('close', common.mustCall(() => {}));
server.bind(0);
