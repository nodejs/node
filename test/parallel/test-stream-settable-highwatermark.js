'use strict';

const common = require('../common');
const assert = require('assert');
const { inspect } = require('util');
const { Duplex, Readable, Writable } = require('stream');

// `readableHighWaterMark` is settable.
{
  const r = new Readable({ read() {} });
  const defaultHwm = r.readableHighWaterMark;
  assert.strictEqual(typeof defaultHwm, 'number');
  r.readableHighWaterMark = 1024;
  assert.strictEqual(r.readableHighWaterMark, 1024);
  assert.strictEqual(r._readableState.highWaterMark, 1024);
}

// `writableHighWaterMark` is settable.
{
  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  const defaultHwm = w.writableHighWaterMark;
  assert.strictEqual(typeof defaultHwm, 'number');
  w.writableHighWaterMark = 1024;
  assert.strictEqual(w.writableHighWaterMark, 1024);
  assert.strictEqual(w._writableState.highWaterMark, 1024);
}

// On a Duplex, each side is independent.
{
  const d = new Duplex({ read() {}, write(chunk, enc, cb) { cb(); } });
  d.readableHighWaterMark = 100;
  d.writableHighWaterMark = 200;
  assert.strictEqual(d.readableHighWaterMark, 100);
  assert.strictEqual(d.writableHighWaterMark, 200);
  assert.strictEqual(d._readableState.highWaterMark, 100);
  assert.strictEqual(d._writableState.highWaterMark, 200);
}

// Invalid values are rejected by the setters.
{
  for (const invalidHwm of [true, false, '5', {}, -5, NaN, 1.5]) {
    const suffix = ` (invalidHwm=${inspect(invalidHwm)})`;
    const err = { name: 'TypeError', code: 'ERR_INVALID_ARG_VALUE' };
    {
      const r = new Readable({ read() {} });
      assert.throws(() => { r.readableHighWaterMark = invalidHwm; }, err,
                    `readableHighWaterMark${suffix}`);
    }
    {
      const w = new Writable({ write(chunk, enc, cb) { cb(); } });
      assert.throws(() => { w.writableHighWaterMark = invalidHwm; }, err,
                    `writableHighWaterMark${suffix}`);
    }
  }
}

// Setting a lower writable high-water mark is respected by backpressure.
{
  let writeCb;
  const w = new Writable({
    highWaterMark: 1024,
    write(chunk, enc, cb) { writeCb = cb; },
  });
  w.writableHighWaterMark = 1;
  // Writing a buffer larger than the HWM triggers backpressure.
  assert.strictEqual(w.write(Buffer.alloc(1024)), false);
  assert.strictEqual(w.writableNeedDrain, true);
  writeCb();
}

// The setters are defined on the prototypes (accessible after construction).
{
  const r = new Readable({ read() {} });
  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  assert.strictEqual(
    Object.hasOwn(Readable.prototype, 'readableHighWaterMark'), true);
  assert.strictEqual(
    Object.hasOwn(Writable.prototype, 'writableHighWaterMark'), true);
  assert.strictEqual(typeof r.readableHighWaterMark, 'number');
  assert.strictEqual(typeof w.writableHighWaterMark, 'number');
}

// A settable high-water mark is used by the stream machinery on the writable
// side without crashing (write respects the new value immediately).
{
  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  w.writableHighWaterMark = 0;
  assert.strictEqual(w.write(Buffer.alloc(0)), true);
  w.end();
  w.on('finish', common.mustCall());
}
