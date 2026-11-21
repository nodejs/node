'use strict';

const common = require('../common');
const assert = require('assert');
const { Duplex, Readable, Transform, Writable } = require('stream');

// `writableDefaultEncoding` reflects the encoding used when writing strings.
{
  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  assert.strictEqual(w.writableDefaultEncoding, 'utf8');

  const w2 = new Writable({
    defaultEncoding: 'latin1',
    write(chunk, enc, cb) { cb(); },
  });
  assert.strictEqual(w2.writableDefaultEncoding, 'latin1');
}

// `writableErrorEmitted` / `readableErrorEmitted` reflect whether `'error'`
// has already been emitted.
{
  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  const r = new Readable({ read() {} });
  assert.strictEqual(w.writableErrorEmitted, false);
  assert.strictEqual(r.readableErrorEmitted, false);

  const wErr = new Error('write boom');
  w.on('error', common.mustCall(() => {
    assert.strictEqual(w.writableErrorEmitted, true);
  }));
  w.destroy(wErr);

  const rErr = new Error('read boom');
  r.on('error', common.mustCall(() => {
    assert.strictEqual(r.readableErrorEmitted, true);
  }));
  r.destroy(rErr);
}

// `readableResumeScheduled` is set once `resume()` is called and cleared once
// the scheduled resume has been processed.
{
  const r = new Readable({ read() {} });
  assert.strictEqual(r.readableResumeScheduled, false);
  r.resume();
  assert.strictEqual(r.readableResumeScheduled, true);
  setImmediate(common.mustCall(() => {
    assert.strictEqual(r.readableResumeScheduled, false);
  }));
}

// `autoDestroy` maps to the autoDestroy option on both Readable and Writable.
{
  const r = new Readable({ read() {} });
  assert.strictEqual(r.autoDestroy, true);
  r.autoDestroy = false;
  assert.strictEqual(r.autoDestroy, false);

  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  assert.strictEqual(w.autoDestroy, true);
  w.autoDestroy = false;
  assert.strictEqual(w.autoDestroy, false);

  const r2 = new Readable({ read() {}, autoDestroy: false });
  assert.strictEqual(r2.autoDestroy, false);
  const w2 = new Writable({ write(chunk, enc, cb) { cb(); }, autoDestroy: false });
  assert.strictEqual(w2.autoDestroy, false);
}

// On a Duplex, `autoDestroy` is one option applied to both sides, so setting
// the accessor must update _readableState and _writableState together.
{
  const d1 = new Duplex({ read() {}, write(chunk, enc, cb) { cb(); } });
  d1.autoDestroy = false;
  assert.strictEqual(d1.autoDestroy, false);
  assert.strictEqual(d1._readableState.autoDestroy, false);
  assert.strictEqual(d1._writableState.autoDestroy, false);

  const d2 = new Duplex({
    read() {},
    write(chunk, enc, cb) { cb(); },
    autoDestroy: false,
  });
  d2.autoDestroy = true;
  assert.strictEqual(d2.autoDestroy, true);
  assert.strictEqual(d2._readableState.autoDestroy, true);
  assert.strictEqual(d2._writableState.autoDestroy, true);
}

// The Duplex autoDestroy accessor actually drives auto-destruction.
{
  const d1 = new Duplex({ read() {}, write(chunk, enc, cb) { cb(); } });
  d1.autoDestroy = false;

  const d2 = new Duplex({
    read() {},
    write(chunk, enc, cb) { cb(); },
    autoDestroy: false,
  });
  d2.autoDestroy = true;

  let remaining = 2;
  const check = common.mustCall(() => {
    assert.strictEqual(d1.destroyed, false);
    assert.strictEqual(d2.destroyed, true);
  });
  function endIt(d) {
    d.resume();
    d.on('end', () => {});
    d.on('close', () => {});
    d.end('x');
    d.push(null);
    setImmediate(() => {
      if (--remaining === 0) {
        setImmediate(check);
      }
    });
  }
  endIt(d1);
  endIt(d2);
}

// A `Transform` extends `Duplex`, so the same `autoDestroy` sync applies.
{
  const t1 = new Transform({ transform(chunk, enc, cb) { cb(null, chunk); } });
  t1.autoDestroy = false;
  assert.strictEqual(t1._readableState.autoDestroy, false);
  assert.strictEqual(t1._writableState.autoDestroy, false);

  const t2 = new Transform({
    transform(chunk, enc, cb) { cb(null, chunk); },
    autoDestroy: false,
  });
  t2.autoDestroy = true;
  assert.strictEqual(t2._readableState.autoDestroy, true);
  assert.strictEqual(t2._writableState.autoDestroy, true);
}

// On a Duplex the writable-side accessors are reachable as well.
{
  const d = new Duplex({
    read() {},
    write(chunk, enc, cb) { cb(); },
    defaultEncoding: 'latin1',
  });
  assert.strictEqual(d.writableDefaultEncoding, 'latin1');
  assert.strictEqual(d.writableErrorEmitted, false);
  assert.strictEqual(d.readableErrorEmitted, false);
}
