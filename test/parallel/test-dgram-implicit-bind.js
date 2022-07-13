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
const dgram = require('dgram');

const source = dgram.createSocket('udp4');
const target = dgram.createSocket('udp4');
let messages = 0;

target.on('message', common.mustCall(function(buf) {
  if (buf.toString() === 'abc') ++messages;
  if (buf.toString() === 'def') ++messages;
  if (messages === 2) {
    source.close();
    target.close();
  }
}, 2));

target.on('listening', common.mustCall(function() {
  // Second .send() call should not throw a bind error.
  const port = this.address().port;
  source.send(Buffer.from('abc'), 0, 3, port, '127.0.0.1');
  source.send(Buffer.from('def'), 0, 3, port, '127.0.0.1');
}));

target.bind(0);
