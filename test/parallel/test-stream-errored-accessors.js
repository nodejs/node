'use strict';

const common = require('../common');
const assert = require('assert');
const { Duplex, Readable, Writable } = require('stream');

// `readableErrored` mirrors the readable side.
{
  const r = new Readable({ read() {} });
  const err = new Error('boom');
  r.on('error', common.mustCall((e) => {
    assert.strictEqual(e, err);
    assert.strictEqual(r.readableErrored, err);
    assert.strictEqual(r.errored, err);
  }));
  r.destroy(err);
}

// `writableErrored` mirrors the writable side.
{
  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  const err = new Error('boom');
  w.on('error', common.mustCall((e) => {
    assert.strictEqual(e, err);
    assert.strictEqual(w.writableErrored, err);
    assert.strictEqual(w.errored, err);
  }));
  w.destroy(err);
}

// On a Duplex, destroying with an error errors both sides.
{
  const d = new Duplex({ read() {}, write(chunk, enc, cb) { cb(); } });
  const err = new Error('boom');
  d.on('error', common.mustCall((e) => {
    assert.strictEqual(e, err);
    assert.strictEqual(d.readableErrored, err);
    assert.strictEqual(d.writableErrored, err);
  }));
  d.destroy(err);
}

// On a Duplex, `errored` reflects the readable side, while `writableErrored`
// exposes the writable side independently.
{
  const d = new Duplex({ read() {}, write(chunk, enc, cb) { cb(); } });
  const rErr = new Error('read boom');
  const wErr = new Error('write boom');

  // Only the readable side errored.
  d._readableState.errored = rErr;
  assert.strictEqual(d.readableErrored, rErr);
  assert.strictEqual(d.errored, rErr);
  assert.strictEqual(d.writableErrored, null);

  // Only the writable side errored.
  d._readableState.errored = null;
  d._writableState.errored = wErr;
  assert.strictEqual(d.writableErrored, wErr);
  assert.strictEqual(d.readableErrored, null);
  // `errored` reflects the readable side, which is not errored here.
  assert.strictEqual(d.errored, null);
}

// The accessors are absent on the side a stream does not have.
{
  const r = new Readable({ read() {} });
  assert.strictEqual(r.readableErrored, null);
  assert.strictEqual(r.writableErrored, undefined);

  const w = new Writable({ write(chunk, enc, cb) { cb(); } });
  assert.strictEqual(w.writableErrored, null);
  assert.strictEqual(w.readableErrored, undefined);
}
