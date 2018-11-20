'use strict';
const common = require('../common');
const assert = require('assert');

const { Transform, Readable, Writable } = require('stream');

const DEFAULT = 16 * 1024;

function testTransform(expectedReadableHwm, expectedWritableHwm, options) {
  const t = new Transform(options);
  assert.strictEqual(t._readableState.highWaterMark, expectedReadableHwm);
  assert.strictEqual(t._writableState.highWaterMark, expectedWritableHwm);
}

// test overriding defaultHwm
testTransform(666, DEFAULT, { readableHighWaterMark: 666 });
testTransform(DEFAULT, 777, { writableHighWaterMark: 777 });
testTransform(666, 777, {
  readableHighWaterMark: 666,
  writableHighWaterMark: 777,
});

// test 0 overriding defaultHwm
testTransform(0, DEFAULT, { readableHighWaterMark: 0 });
testTransform(DEFAULT, 0, { writableHighWaterMark: 0 });

// test highWaterMark overriding
testTransform(555, 555, {
  highWaterMark: 555,
  readableHighWaterMark: 666,
});
testTransform(555, 555, {
  highWaterMark: 555,
  writableHighWaterMark: 777,
});
testTransform(555, 555, {
  highWaterMark: 555,
  readableHighWaterMark: 666,
  writableHighWaterMark: 777,
});

// test highWaterMark = 0 overriding
testTransform(0, 0, {
  highWaterMark: 0,
  readableHighWaterMark: 666,
});
testTransform(0, 0, {
  highWaterMark: 0,
  writableHighWaterMark: 777,
});
testTransform(0, 0, {
  highWaterMark: 0,
  readableHighWaterMark: 666,
  writableHighWaterMark: 777,
});

// test undefined, null
[undefined, null].forEach((v) => {
  testTransform(DEFAULT, DEFAULT, { readableHighWaterMark: v });
  testTransform(DEFAULT, DEFAULT, { writableHighWaterMark: v });
  testTransform(666, DEFAULT, { highWaterMark: v, readableHighWaterMark: 666 });
  testTransform(DEFAULT, 777, { highWaterMark: v, writableHighWaterMark: 777 });
});

// test NaN
{
  common.expectsError(() => {
    new Transform({ readableHighWaterMark: NaN });
  }, {
    type: TypeError,
    code: 'ERR_INVALID_OPT_VALUE',
    message: 'The value "NaN" is invalid for option "readableHighWaterMark"'
  });

  common.expectsError(() => {
    new Transform({ writableHighWaterMark: NaN });
  }, {
    type: TypeError,
    code: 'ERR_INVALID_OPT_VALUE',
    message: 'The value "NaN" is invalid for option "writableHighWaterMark"'
  });
}

// test non Duplex streams ignore the options
{
  const r = new Readable({ readableHighWaterMark: 666 });
  assert.strictEqual(r._readableState.highWaterMark, DEFAULT);
  const w = new Writable({ writableHighWaterMark: 777 });
  assert.strictEqual(w._writableState.highWaterMark, DEFAULT);
}
