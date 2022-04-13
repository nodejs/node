'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// fs.write with length > INT32_MAX

common.skipIf32Bits();

let buf;
try {
  buf = Buffer.allocUnsafe(0x7FFFFFFF + 1);
} catch (e) {
  // If the exception is not due to memory confinement then rethrow it.
  if (e.message !== 'Array buffer allocation failed') throw (e);
  common.skip('skipped due to memory requirements');
}

const filename = path.join(tmpdir.path, 'write9.txt');
fs.open(filename, 'w', 0o644, common.mustSucceed((fd) => {
  assert.throws(() => {
    fs.write(fd,
             buf,
             0,
             0x7FFFFFFF + 1,
             0,
             common.mustNotCall());
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "length" is out of range. ' +
      'It must be >= 0 && <= 2147483647. Received 2147483648'
  });

  fs.closeSync(fd);
}));
