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
const expected = Buffer.from('hello');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// fs.write with all parameters provided:
{
  const filename = tmpdir.resolve('write1.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, expected.length);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(found, expected.toString());
    });

    fs.write(fd, expected, 0, expected.length, null, cb);
  }));
}

// fs.write with a buffer, without the length parameter:
{
  const filename = tmpdir.resolve('write2.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, 2);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(found, 'lo');
    });

    fs.write(fd, Buffer.from('hello'), 3, cb);
  }));
}

// fs.write with a buffer, without the offset and length parameters:
{
  const filename = tmpdir.resolve('write3.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, expected.length);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(expected.toString(), found);
    });

    fs.write(fd, expected, cb);
  }));
}

// fs.write with the offset passed as undefined followed by the callback:
{
  const filename = tmpdir.resolve('write4.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, expected.length);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(expected.toString(), found);
    });

    fs.write(fd, expected, undefined, cb);
  }));
}

// fs.write with offset and length passed as undefined followed by the callback:
{
  const filename = tmpdir.resolve('write5.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, expected.length);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(found, expected.toString());
    });

    fs.write(fd, expected, undefined, undefined, cb);
  }));
}

// fs.write with a Uint8Array, without the offset and length parameters:
{
  const filename = tmpdir.resolve('write6.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, expected.length);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(found, expected.toString());
    });

    fs.write(fd, Uint8Array.from(expected), cb);
  }));
}

// fs.write with invalid offset type
{
  const filename = tmpdir.resolve('write7.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    assert.throws(() => {
      fs.write(fd,
               Buffer.from('abcd'),
               NaN,
               expected.length,
               0,
               common.mustNotCall());
    }, {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "offset" is out of range. ' +
               'It must be an integer. Received NaN'
    });

    fs.closeSync(fd);
  }));
}

// fs.write with a DataView, without the offset and length parameters:
{
  const filename = tmpdir.resolve('write8.txt');
  fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
    const cb = common.mustSucceed((written) => {
      assert.strictEqual(written, expected.length);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(found, expected.toString());
    });

    const uint8 = Uint8Array.from(expected);
    fs.write(fd, new DataView(uint8.buffer), cb);
  }));
}
