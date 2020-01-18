'use strict';

const common = require('../common');
const { Writable, Readable, Transform, finished } = require('stream');
const assert = require('assert');
const EE = require('events');
const fs = require('fs');
const { promisify } = require('util');

{
  const rs = new Readable({
    read() {}
  });

  finished(rs, common.mustCall((err) => {
    assert(!err, 'no error');
  }));

  rs.push(null);
  rs.resume();
}

{
  const ws = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  finished(ws, common.mustCall((err) => {
    assert(!err, 'no error');
  }));

  ws.end();
}

{
  const tr = new Transform({
    transform(data, enc, cb) {
      cb();
    }
  });

  let finish = false;
  let ended = false;

  tr.on('end', () => {
    ended = true;
  });

  tr.on('finish', () => {
    finish = true;
  });

  finished(tr, common.mustCall((err) => {
    assert(!err, 'no error');
    assert(finish);
    assert(ended);
  }));

  tr.end();
  tr.resume();
}

{
  const rs = fs.createReadStream(__filename);

  rs.resume();
  finished(rs, common.mustCall());
}

{
  const finishedPromise = promisify(finished);

  async function run() {
    const rs = fs.createReadStream(__filename);
    const done = common.mustCall();

    let ended = false;
    rs.resume();
    rs.on('end', () => {
      ended = true;
    });
    await finishedPromise(rs);
    assert(ended);
    done();
  }

  run();
}

{
  const rs = fs.createReadStream('file-does-not-exist');

  finished(rs, common.expectsError({
    code: 'ENOENT'
  }));
}

{
  const rs = new Readable();

  finished(rs, common.mustCall((err) => {
    assert(err, 'premature close error');
  }));

  rs.push(null);
  rs.emit('close');
  rs.resume();
}

{
  const rs = new Readable();

  finished(rs, common.mustCall((err) => {
    assert(!err, 'no error');
  }));

  rs.push(null);
  rs.on('end', common.mustCall(() => {
    rs.emit('close'); // Should not trigger an error
  }));
  rs.resume();
}

{
  const rs = new Readable();

  finished(rs, common.mustCall((err) => {
    assert(err, 'premature close error');
  }));

  rs.emit('close'); // Should trigger error
  rs.push(null);
  rs.resume();
}

// Test faulty input values and options.
{
  const rs = new Readable({
    read() {}
  });

  assert.throws(
    () => finished(rs, 'foo'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /callback/
    }
  );
  assert.throws(
    () => finished(rs, 'foo', () => {}),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /opts/
    }
  );
  assert.throws(
    () => finished(rs, {}, 'foo'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /callback/
    }
  );

  finished(rs, null, common.mustCall());

  rs.push(null);
  rs.resume();
}

{
  // Nothing happens if disposed.

  const ws = new Writable({
    write(data, env, cb) {
      cb();
    }
  });
  const removeListener = finished(ws, common.mustNotCall());
  removeListener();
  ws.end();
}

{
  // Nothing happens if disposed.

  const rs = new Readable();
  const removeListeners = finished(rs, common.mustNotCall());
  removeListeners();

  rs.emit('close');
  rs.push(null);
  rs.resume();
}

{
  // Completed if readable-like is ended before.

  let ticked = false;
  const streamLike = new EE();
  streamLike.readableEnded = true;
  streamLike.readable = true;
  finished(streamLike, common.mustCall(() => {
    assert.strictEqual(ticked, true);
  }));
  ticked = true;
}

{
  // Completed if readable-like is never ended.

  const streamLike = new EE();
  streamLike.readableEnded = false;
  streamLike.readable = true;
  finished(streamLike, common.expectsError({
    code: 'ERR_STREAM_PREMATURE_CLOSE'
  }));
  streamLike.emit('close');
}

{
  // Completed if streamlike is finished before.

  const streamLike = new EE();
  streamLike.writableFinished = true;
  streamLike.writable = true;
  finished(streamLike, common.mustCall());
}

{
  // Premature close if stream is not finished.

  const streamLike = new EE();
  streamLike.writableFinished = false;
  streamLike.writable = true;
  finished(streamLike, common.expectsError({
    code: 'ERR_STREAM_PREMATURE_CLOSE'
  }));
  streamLike.emit('close');
}

{
  // No error if stream never emitted 'end'
  // even if readableEnded says something else.

  const streamLike = new EE();
  streamLike.writable = true;
  finished(streamLike, common.mustCall((err) => {
    assert.strictEqual(err, undefined);
  }));
  streamLike.writableFinished = true;
  streamLike.emit('close');
}

{
  // No error if stream never emitted 'finished'
  // even if writableFinished says something else.

  const streamLike = new EE();
  streamLike.readable = true;
  finished(streamLike, common.mustCall((err) => {
    assert.strictEqual(err, undefined);
  }));
  streamLike.readableEnded = true;
  streamLike.emit('close');
}

{
  // Completes if already finished.

  const w = new Writable();
  finished(w, common.mustCall(() => {
    finished(w, common.mustCall());
  }));
  w.destroy();
}

{
  // Completes if already ended.

  const r = new Readable();
  finished(r, common.mustCall(() => {
    finished(r, common.mustCall());
  }));
  r.destroy();
}
