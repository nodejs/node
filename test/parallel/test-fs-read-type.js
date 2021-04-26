'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';


// Error must be thrown with string
assert.throws(
  () => fs.read(fd, expected.length, 0, 'utf-8', common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "buffer" argument must be an instance of Buffer, ' +
             'TypedArray, or DataView. Received type number (4)'
  }
);

[true, null, undefined, () => {}, {}].forEach((value) => {
  assert.throws(() => {
    fs.read(value,
            Buffer.allocUnsafe(expected.length),
            0,
            expected.length,
            0,
            common.mustNotCall());
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });
});

assert.throws(() => {
  fs.read(fd,
          Buffer.allocUnsafe(expected.length),
          -1,
          expected.length,
          0,
          common.mustNotCall());
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
});

assert.throws(() => {
  fs.read(fd,
          Buffer.allocUnsafe(expected.length),
          NaN,
          expected.length,
          0,
          common.mustNotCall());
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "offset" is out of range. It must be an integer. ' +
           'Received NaN'
});

assert.throws(() => {
  fs.read(fd,
          Buffer.allocUnsafe(expected.length),
          0,
          -1,
          0,
          common.mustNotCall());
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "length" is out of range. ' +
           'It must be >= 0. Received -1'
});

[true, () => {}, {}, ''].forEach((value) => {
  assert.throws(() => {
    fs.read(fd,
            Buffer.allocUnsafe(expected.length),
            0,
            expected.length,
            value,
            common.mustNotCall());
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });
});

[0.5, 2 ** 53, 2n ** 63n].forEach((value) => {
  assert.throws(() => {
    fs.read(fd,
            Buffer.allocUnsafe(expected.length),
            0,
            expected.length,
            value,
            common.mustNotCall());
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError'
  });
});

fs.read(fd,
        Buffer.allocUnsafe(expected.length),
        0,
        expected.length,
        0n,
        common.mustSucceed());

fs.read(fd,
        Buffer.allocUnsafe(expected.length),
        0,
        expected.length,
        2n ** 53n - 1n,
        common.mustCall((err) => {
          if (err) {
            if (common.isIBMi)
              assert.strictEqual(err.errno, -127);
            else
              assert.strictEqual(err.code, 'EFBIG');
          }
        }));

assert.throws(
  () => fs.readSync(fd, expected.length, 0, 'utf-8'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "buffer" argument must be an instance of Buffer, ' +
             'TypedArray, or DataView. Received type number (4)'
  }
);

[true, null, undefined, () => {}, {}].forEach((value) => {
  assert.throws(() => {
    fs.readSync(value,
                Buffer.allocUnsafe(expected.length),
                0,
                expected.length,
                0);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });
});

assert.throws(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              -1,
              expected.length,
              0);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
});

assert.throws(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              NaN,
              expected.length,
              0);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "offset" is out of range. It must be an integer. ' +
           'Received NaN'
});

assert.throws(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              0,
              -1,
              0);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "length" is out of range. ' +
           'It must be >= 0. Received -1'
});

assert.throws(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              0,
              expected.length + 1,
              0);
}, {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "length" is out of range. ' +
           'It must be <= 4. Received 5'
});

[true, () => {}, {}, ''].forEach((value) => {
  assert.throws(() => {
    fs.readSync(fd,
                Buffer.allocUnsafe(expected.length),
                0,
                expected.length,
                value);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });
});

[0.5, 2 ** 53, 2n ** 63n].forEach((value) => {
  assert.throws(() => {
    fs.readSync(fd,
                Buffer.allocUnsafe(expected.length),
                0,
                expected.length,
                value);
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError'
  });
});

fs.readSync(fd,
            Buffer.allocUnsafe(expected.length),
            0,
            expected.length,
            0n);

try {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              0,
              expected.length,
              2n ** 53n - 1n);
} catch (err) {
  // On systems where max file size is below 2^53-1, we'd expect a EFBIG error.
  // This is not using `assert.throws` because the above call should not raise
  // any error on systems that allows file of that size.
  if (err.code !== 'EFBIG' && !(common.isIBMi && err.errno === -127))
    throw err;
}
