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
const zlib = require('zlib');

// String "test" encoded with dictionary "dict".
const input = Buffer.from([0x78, 0xBB, 0x04, 0x09, 0x01, 0xA5]);

{
  const stream = zlib.createInflate();

  stream.on('error', common.mustCall(function(err) {
    assert(/Missing dictionary/.test(err.message));
  }));

  stream.write(input);
}

{
  const stream = zlib.createInflate({ dictionary: Buffer.from('fail') });

  stream.on('error', common.mustCall(function(err) {
    assert(/Bad dictionary/.test(err.message));
  }));

  stream.write(input);
}

{
  const stream = zlib.createInflateRaw({ dictionary: Buffer.from('fail') });

  stream.on('error', common.mustCall(function(err) {
    // It's not possible to separate invalid dict and invalid data when using
    // the raw format
    assert(/invalid/.test(err.message));
  }));

  stream.write(input);
}
