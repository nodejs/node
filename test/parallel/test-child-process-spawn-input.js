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

const spawn = require('child_process').spawn;

const badOptions = {
  input: 1234
};

assert.throws(
  () => spawn('cat', [], badOptions),
  { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });

const stringOptions = {
  input: 'hello world'
};

const catString = spawn('cat', [], stringOptions);

catString.stdout.on('data', common.mustCall((data) => {
  assert.strictEqual(data.toString('utf8'), stringOptions.input);
}));

catString.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}));

const bufferOptions = {
  input: Buffer.from('hello world')
};

const catBuffer = spawn('cat', [], bufferOptions);

catBuffer.stdout.on('data', common.mustCall((data) => {
  assert.deepStrictEqual(data, bufferOptions.input);
}));

catBuffer.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}));

// common.getArrayBufferViews expects a buffer
// with length an multiple of 8
const msgBuf = Buffer.from('hello world'.repeat(8));
for (const arrayBufferView of common.getArrayBufferViews(msgBuf)) {
  const options = {
    input: arrayBufferView
  };

  const catArrayBufferView = spawn('cat', [], options);

  catArrayBufferView.stdout.on('data', common.mustCall((data) => {
    assert.deepStrictEqual(data, msgBuf);
  }));

  catArrayBufferView.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}
