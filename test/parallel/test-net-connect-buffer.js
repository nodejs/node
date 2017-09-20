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

let dataWritten = false;
let connectHappened = false;

const tcp = net.Server(function(s) {
  tcp.close();

  console.log('tcp server connection');

  let buf = '';
  s.on('data', function(d) {
    buf += d;
  });

  s.on('end', function() {
    console.error('SERVER: end', buf.toString());
    assert.strictEqual(buf, "L'État, c'est moi");
    console.log('tcp socket disconnect');
    s.end();
  });

  s.on('error', function(e) {
    console.log(`tcp server-side error: ${e.message}`);
    process.exit(1);
  });
});

tcp.listen(0, function() {
  const socket = net.Stream({ highWaterMark: 0 });

  console.log('Connecting to socket ');

  socket.connect(this.address().port, function() {
    console.log('socket connected');
    connectHappened = true;
  });

  console.log(`connecting = ${socket.connecting}`);

  assert.strictEqual('opening', socket.readyState);

  // Make sure that anything besides a buffer or a string throws.
  [null,
   true,
   false,
   undefined,
   1,
   1.0,
   1 / 0,
   +Infinity,
   -Infinity,
   [],
   {}
  ].forEach(function(v) {
    function f() {
      console.error('write', v);
      socket.write(v);
    }
    assert.throws(f, TypeError);
  });

  // Write a string that contains a multi-byte character sequence to test that
  // `bytesWritten` is incremented with the # of bytes, not # of characters.
  const a = "L'État, c'est ";
  const b = 'moi';

  // We're still connecting at this point so the datagram is first pushed onto
  // the connect queue. Make sure that it's not added to `bytesWritten` again
  // when the actual write happens.
  const r = socket.write(a, function(er) {
    console.error('write cb');
    dataWritten = true;
    assert.ok(connectHappened);
    console.error('socket.bytesWritten', socket.bytesWritten);
    //assert.strictEqual(socket.bytesWritten, Buffer.from(a + b).length);
    console.error('data written');
  });
  console.error('socket.bytesWritten', socket.bytesWritten);
  console.error('write returned', r);

  assert.strictEqual(socket.bytesWritten, Buffer.from(a).length);

  assert.strictEqual(false, r);
  socket.end(b);

  assert.strictEqual('opening', socket.readyState);
});

process.on('exit', function() {
  assert.ok(connectHappened);
  assert.ok(dataWritten);
});
