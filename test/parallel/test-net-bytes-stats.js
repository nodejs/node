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

let bytesRead = 0;
let bytesWritten = 0;
let count = 0;

const tcp = net.Server(function(s) {
  console.log('tcp server connection');

  // trigger old mode.
  s.resume();

  s.on('end', function() {
    bytesRead += s.bytesRead;
    console.log(`tcp socket disconnect #${count}`);
  });
});

tcp.listen(0, function doTest() {
  console.error('listening');
  const socket = net.createConnection(this.address().port);

  socket.on('connect', function() {
    count++;
    console.error('CLIENT connect #%d', count);

    socket.write('foo', function() {
      console.error('CLIENT: write cb');
      socket.end('bar');
    });
  });

  socket.on('finish', function() {
    bytesWritten += socket.bytesWritten;
    console.error('CLIENT end event #%d', count);
  });

  socket.on('close', function() {
    console.error('CLIENT close event #%d', count);
    console.log(`Bytes read: ${bytesRead}`);
    console.log(`Bytes written: ${bytesWritten}`);
    if (count < 2) {
      console.error('RECONNECTING');
      socket.connect(tcp.address().port);
    } else {
      tcp.close();
    }
  });
});

process.on('exit', function() {
  assert.strictEqual(bytesRead, 12);
  assert.strictEqual(bytesWritten, 12);
});
