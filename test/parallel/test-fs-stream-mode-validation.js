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
const fs = require('fs');

// Regression test for https://github.com/nodejs/node/issues/62516
// Passing a value that exceeds Int32 max (but is within UInt32 range) to
// createWriteStream/createReadStream mode option should throw RangeError,
// not crash with assertion failure.

const INT32_MAX = 2147483647;
const VALUE_EXCEEDS_INT32 = 2176057344;  // Within UInt32, exceeds Int32

let testsCompleted = 0;
const expectedTests = 4;

function checkCompletion() {
  testsCompleted++;
  if (testsCompleted === expectedTests) {
    console.log('ok');
  }
}

// Test createWriteStream with mode value exceeding Int32 max
{
  const stream = fs.createWriteStream('/tmp/test-write.txt', {
    mode: VALUE_EXCEEDS_INT32,
  });
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_OUT_OF_RANGE');
    assert.strictEqual(err.name, 'RangeError');
    assert(/The value of "mode" is out of range/.test(err.message));
    checkCompletion();
  }));
}

// Test createReadStream with mode value exceeding Int32 max
{
  const stream = fs.createReadStream('/tmp/test-read.txt', {
    mode: VALUE_EXCEEDS_INT32,
  });
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_OUT_OF_RANGE');
    assert.strictEqual(err.name, 'RangeError');
    assert(/The value of "mode" is out of range/.test(err.message));
    checkCompletion();
  }));
}

// Test that valid mode values still work
{
  const writeStream = fs.createWriteStream('/tmp/test-valid.txt', {
    mode: 0o666,
  });
  writeStream.on('ready', () => {
    writeStream.destroy();
    checkCompletion();
  });
  writeStream.write('test');
}

// Test boundary value (Int32 max should be accepted)
{
  const writeStream = fs.createWriteStream('/tmp/test-boundary.txt', {
    mode: INT32_MAX,
  });
  writeStream.on('ready', () => {
    writeStream.destroy();
    checkCompletion();
  });
  writeStream.write('test');
}
