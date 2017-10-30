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

// Flags: --expose_internals
'use strict';
require('../common');
const assert = require('assert');
const fromList = require('_stream_readable')._fromList;
const BufferList = require('internal/streams/BufferList');

function bufferListFromArray(arr) {
  const bl = new BufferList();
  for (let i = 0; i < arr.length; ++i)
    bl.push(arr[i]);
  return bl;
}

{
  // Verify behavior with buffers
  let list = [ Buffer.from('foog'),
               Buffer.from('bark'),
               Buffer.from('bazy'),
               Buffer.from('kuel') ];
  list = bufferListFromArray(list);

  // read more than the first element.
  let ret = fromList(6, { buffer: list, length: 16 });
  assert.strictEqual(ret.toString(), 'foogba');

  // read exactly the first element.
  ret = fromList(2, { buffer: list, length: 10 });
  assert.strictEqual(ret.toString(), 'rk');

  // read less than the first element.
  ret = fromList(2, { buffer: list, length: 8 });
  assert.strictEqual(ret.toString(), 'ba');

  // read more than we have.
  ret = fromList(100, { buffer: list, length: 6 });
  assert.strictEqual(ret.toString(), 'zykuel');

  // all consumed.
  assert.deepStrictEqual(list, new BufferList());
}

{
  // Verify behavior with strings
  let list = [ 'foog',
               'bark',
               'bazy',
               'kuel' ];
  list = bufferListFromArray(list);

  // read more than the first element.
  let ret = fromList(6, { buffer: list, length: 16, decoder: true });
  assert.strictEqual(ret, 'foogba');

  // read exactly the first element.
  ret = fromList(2, { buffer: list, length: 10, decoder: true });
  assert.strictEqual(ret, 'rk');

  // read less than the first element.
  ret = fromList(2, { buffer: list, length: 8, decoder: true });
  assert.strictEqual(ret, 'ba');

  // read more than we have.
  ret = fromList(100, { buffer: list, length: 6, decoder: true });
  assert.strictEqual(ret, 'zykuel');

  // all consumed.
  assert.deepStrictEqual(list, new BufferList());
}
