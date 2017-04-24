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
// Ensure that if a dgram socket is closed before the DNS lookup completes, it
// won't crash.

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const buf = Buffer.alloc(1024, 42);

let socket = dgram.createSocket('udp4');
const handle = socket._handle;

// get a random port for send
const portGetter = dgram.createSocket('udp4')
  .bind(0, 'localhost', common.mustCall(() => {
    socket.send(buf, 0, buf.length,
                portGetter.address().port,
                portGetter.address().address);

    assert.strictEqual(socket.close(common.mustCall()), socket);
    socket.on('close', common.mustCall());
    socket = null;

    // Verify that accessing handle after closure doesn't throw
    setImmediate(function() {
      setImmediate(function() {
        console.log('Handle fd is: ', handle.fd);
      });
    });

    portGetter.close();
  }));
