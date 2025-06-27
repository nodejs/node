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
const net = require('net');

const tcp = net.Server(common.mustCall((s) => {
  tcp.close();

  let buf = '';
  s.setEncoding('utf8');
  s.on('data', function(d) {
    buf += d;
  });

  s.on('end', common.mustCall(function() {
    console.error('SERVER: end', buf);
    assert.strictEqual(buf, "L'État, c'est moi");
    s.end();
  }));
}));

tcp.listen(0, common.mustCall(function() {
  const socket = net.Stream({ highWaterMark: 0 });

  let connected = false;
  assert.strictEqual(socket.pending, true);
  socket.connect(this.address().port, common.mustCall(() => connected = true));

  assert.strictEqual(socket.pending, true);
  assert.strictEqual(socket.connecting, true);
  assert.strictEqual(socket.readyState, 'opening');

  // Write a string that contains a multi-byte character sequence to test that
  // `bytesWritten` is incremented with the # of bytes, not # of characters.
  const a = "L'État, c'est ";
  const b = 'moi';

  // We're still connecting at this point so the datagram is first pushed onto
  // the connect queue. Make sure that it's not added to `bytesWritten` again
  // when the actual write happens.
  const r = socket.write(a, common.mustCall((er) => {
    console.error('write cb');
    assert.ok(connected);
    assert.strictEqual(socket.bytesWritten, Buffer.from(a + b).length);
    assert.strictEqual(socket.pending, false);
  }));
  socket.on('close', common.mustCall(() => {
    assert.strictEqual(socket.pending, true);
  }));

  assert.strictEqual(socket.bytesWritten, Buffer.from(a).length);
  assert.strictEqual(r, false);
  socket.end(b);

  assert.strictEqual(socket.readyState, 'opening');
}));
