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
const fixtures = require('../common/fixtures');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');

//fs.read expects incorrect fd parameter
function testFd(bufferAsync, offset, bufferSync, expected) {
  common.expectsError(
    fs.read.bind(fd,
                 bufferAsync,
                 offset,
                 expected.length,
                 0,
                 common.mustNotCall()),
    { code: 'ERR_INVALID_ARG_TYPE', type: TypeError }
  );
}

//fs.read expects fd parameter out of range
function testFdRangeOut(bufferAsync, offset, bufferSync, expected) {
  common.expectsError(
    fs.read.bind(undefined,
                 -1,
                 bufferAsync,
                 offset,
                 expected.length,
                 0,
                 common.mustNotCall()),
    { code: 'ERR_OUT_OF_RANGE', type: RangeError }
  );
}

//fs.read expects offset parameter out of range
function testOffSet(bufferAsync, offset, length, expected) {
  common.expectsError(
    fs.read.bind(undefined,
                 fd,
                 bufferAsync,
                 offset,
                 expected.length,
                 0,
                 common.mustNotCall()),
    { code: 'ERR_OUT_OF_RANGE', type: RangeError }
  );
}

//fs.read expects length parameter out of range
function testLenght(bufferAsync, offset, length) {
  common.expectsError(
    fs.read.bind(undefined,
                 fd,
                 bufferAsync,
                 offset,
                 length,
                 0,
                 common.mustNotCall()),
    { code: 'ERR_OUT_OF_RANGE', type: RangeError }
  );
}

//test fd parameter
testFd(Buffer.allocUnsafe(expected.length),
       0,
       Buffer.allocUnsafe(expected.length),
       expected);

//test fd parameter out of range
testFdRangeOut(Buffer.allocUnsafe(expected.length),
               0,
               Buffer.allocUnsafe(expected.length),
               expected);

//test offset parameter out of range
testOffSet(Buffer.allocUnsafe(expected.length),
           -1,
           Buffer.allocUnsafe(expected.length),
           expected);

//test length parameter out of range
testLenght(Buffer.allocUnsafe(expected.length),
           0,
           -1);
