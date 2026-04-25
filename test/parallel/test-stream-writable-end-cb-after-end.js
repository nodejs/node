'use strict';

// Regression test for https://github.com/nodejs/node/issues/33684
// Calling `.end(cb)` on a stream that has already been ended must invoke
// the user-supplied callback with ERR_STREAM_WRITE_AFTER_END, mirroring
// the behavior of `.write(chunk, cb)` after end and `.end(chunk, cb)`
// after end.

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');

{
  // `.end()` followed by `.end(cb)` — cb must receive ERR_STREAM_WRITE_AFTER_END.
  const w = new Writable({
    write(chunk, encoding, cb) { cb(); },
  });

  w.end();
  w.end(common.mustCall((err) => {
    assert.ok(err);
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
}

{
  // `.end()` followed by `.end('chunk', cb)` — cb must receive
  // ERR_STREAM_WRITE_AFTER_END (existing behavior, included for parity).
  const w = new Writable({
    write(chunk, encoding, cb) { cb(); },
  });

  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));

  w.end();
  w.end('chunk', common.mustCall((err) => {
    assert.ok(err);
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
}

{
  // `.write(chunk, cb)` after end calls cb with ERR_STREAM_WRITE_AFTER_END —
  // sanity check confirming that `.end(cb)` is now consistent with this path.
  const w = new Writable({
    write(chunk, encoding, cb) { cb(); },
  });

  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));

  w.end();
  w.write('chunk', common.mustCall((err) => {
    assert.ok(err);
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
}

{
  // `.end(null, encoding, cb)` after end — cb must receive
  // ERR_STREAM_WRITE_AFTER_END.
  const w = new Writable({
    write(chunk, encoding, cb) { cb(); },
  });

  w.end();
  w.end(null, null, common.mustCall((err) => {
    assert.ok(err);
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
}
