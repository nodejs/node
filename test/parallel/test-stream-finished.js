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
    assert(!err, 'no error');
  }));

  rs.push(null);
  rs.emit('close'); // Should not trigger an error
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

// Test that calling returned function removes listeners
{
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
  const rs = new Readable();
  const removeListeners = finished(rs, common.mustNotCall());
  removeListeners();

  rs.emit('close');
  rs.push(null);
  rs.resume();
}

{
  const streamLike = new EE();
  streamLike.readableEnded = true;
  streamLike.readable = true;
  finished(streamLike, common.mustCall());
  streamLike.emit('close');
}

{
  const writable = new Writable({ write() {} });
  writable.writable = false;
  writable.destroy();
  finished(writable, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
}

{
  const readable = new Readable();
  readable.readable = false;
  readable.destroy();
  finished(readable, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
}

{
  const w = new Writable({
    write(chunk, encoding, callback) {
      setImmediate(callback);
    }
  });
  finished(w, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
  w.end('asd');
  w.destroy();
}

function testClosed(factory) {
  {
    // If already destroyed but finished is cancelled in same tick
    // don't invoke the callback,

    const s = factory();
    s.destroy();
    const dispose = finished(s, common.mustNotCall());
    dispose();
  }

  {
    // If already destroyed invoked callback.

    const s = factory();
    s.destroy();
    finished(s, common.mustCall());
  }

  {
    // Don't invoke until destroy has completed.

    let destroyed = false;
    const s = factory({
      destroy(err, cb) {
        setImmediate(() => {
          destroyed = true;
          cb();
        });
      }
    });
    s.destroy();
    finished(s, common.mustCall(() => {
      assert.strictEqual(destroyed, true);
    }));
  }

  {
    // Invoke callback even if close is inhibited.

    const s = factory({
      emitClose: false,
      destroy(err, cb) {
        cb();
        finished(s, common.mustCall());
      }
    });
    s.destroy();
  }

  {
    // Invoke with deep async.

    const s = factory({
      destroy(err, cb) {
        setImmediate(() => {
          cb();
          setImmediate(() => {
            finished(s, common.mustCall());
          });
        });
      }
    });
    s.destroy();
  }
}

testClosed((opts) => new Readable({ ...opts }));
testClosed((opts) => new Writable({ write() {}, ...opts }));

{
  const w = new Writable({
    write(chunk, encoding, cb) {
      cb();
    },
    autoDestroy: false
  });
  w.end('asd');
  process.nextTick(() => {
    finished(w, common.mustCall());
  });
}

{
  const w = new Writable({
    write(chunk, encoding, cb) {
      cb(new Error());
    },
    autoDestroy: false
  });
  w.write('asd');
  w.on('error', common.mustCall(() => {
    finished(w, common.mustCall());
  }));
}

{
  const r = new Readable({
    autoDestroy: false
  });
  r.push(null);
  r.resume();
  r.on('end', common.mustCall(() => {
    finished(r, common.mustCall());
  }));
}

{
  const rs = fs.createReadStream(__filename, { autoClose: false });
  rs.resume();
  rs.on('close', common.mustNotCall());
  rs.on('end', common.mustCall(() => {
    finished(rs, common.mustCall());
  }));
}

{
  const d = new EE();
  d._writableState = {};
  d._writableState.finished = true;
  finished(d, { readable: false, writable: true }, common.mustCall((err) => {
    assert.strictEqual(err, undefined);
  }));
  d._writableState.errored = true;
  d.emit('close');
}

{
  const r = new Readable();
  finished(r, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
  r.push('asd');
  r.push(null);
  r.destroy();
}
