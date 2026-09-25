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

const buf = Buffer.from('test');
const host = '127.0.0.1';
const sock = dgram.createSocket('udp4');

function checkArgs(connected) {
  // First argument should be a buffer.
  assert.throws(
    () => { sock.send(); },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "buffer" argument must be of type string or an instance ' +
      'of Buffer, TypedArray, or DataView. Received undefined'
    }
  );

  // send(buf, offset, length, port, host)
  if (connected) {
    assert.throws(
      () => { sock.send(buf, 1, 1, -1, host); },
      {
        code: 'ERR_SOCKET_DGRAM_IS_CONNECTED',
        name: 'Error',
        message: 'Already connected'
      }
    );

    assert.throws(
      () => { sock.send(buf, 1, 1, 0, host); },
      {
        code: 'ERR_SOCKET_DGRAM_IS_CONNECTED',
        name: 'Error',
        message: 'Already connected'
      }
    );

    assert.throws(
      () => { sock.send(buf, 1, 1, 65536, host); },
      {
        code: 'ERR_SOCKET_DGRAM_IS_CONNECTED',
        name: 'Error',
        message: 'Already connected'
      }
    );

    assert.throws(
      () => { sock.send(buf, 1234, '127.0.0.1', common.mustNotCall()); },
      {
        code: 'ERR_SOCKET_DGRAM_IS_CONNECTED',
        name: 'Error',
        message: 'Already connected'
      }
    );

    const longArray = [1, 2, 3, 4, 5, 6, 7, 8];
    for (const input of ['hello',
                         Buffer.from('hello'),
                         Buffer.from('hello world').subarray(0, 5),
                         Buffer.from('hello world').subarray(4, 9),
                         Buffer.from('hello world').subarray(6),
                         new Uint8Array([1, 2, 3, 4, 5]),
                         new Uint8Array(longArray).subarray(0, 5),
                         new Uint8Array(longArray).subarray(2, 7),
                         new Uint8Array(longArray).subarray(3),
                         new DataView(new ArrayBuffer(5), 0),
                         new DataView(new ArrayBuffer(6), 1),
                         new DataView(new ArrayBuffer(7), 1, 5)]) {
      assert.throws(
        () => { sock.send(input, 6, 0); },
        {
          code: 'ERR_BUFFER_OUT_OF_BOUNDS',
          name: 'RangeError',
          message: '"offset" is outside of buffer bounds',
        }
      );

      assert.throws(
        () => { sock.send(input, 0, 6); },
        {
          code: 'ERR_BUFFER_OUT_OF_BOUNDS',
          name: 'RangeError',
          message: '"length" is outside of buffer bounds',
        }
      );

      assert.throws(
        () => { sock.send(input, 3, 4); },
        {
          code: 'ERR_BUFFER_OUT_OF_BOUNDS',
          name: 'RangeError',
          message: '"length" is outside of buffer bounds',
        }
      );
    }
  } else {
    assert.throws(() => { sock.send(buf, 1, 1, -1, host); }, RangeError);
    assert.throws(() => { sock.send(buf, 1, 1, 0, host); }, RangeError);
    assert.throws(() => { sock.send(buf, 1, 1, 65536, host); }, RangeError);
  }

  // send(buf, port, host)
  assert.throws(
    () => { sock.send(23, 12345, host); },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "buffer" argument must be of type string or an instance ' +
      'of Buffer, TypedArray, or DataView. Received type number (23)'
    }
  );

  // send([buf1, ..], port, host)
  assert.throws(
    () => { sock.send([buf, 23], 12345, host); },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "buffer list arguments" argument must be of type string ' +
      'or an instance of Buffer, TypedArray, or DataView. ' +
      'Received an instance of Array'
    }
  );
}

checkArgs();
sock.connect(12345, common.mustCall(() => {
  checkArgs(true);
  sock.close();
}));
