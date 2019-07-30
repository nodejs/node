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

{
  const streamLike = new EE();
  streamLike.readableEnded = true;
  streamLike.readable = true;
  finished(streamLike, common.mustCall);
  checkListeners(streamLike, 'legacy');
  streamLike.emit('close');
}

// Test that calling returned function removes listeners
{
  const ws = new Writable({
    write(data, env, cb) {
      cb();
    }
  });
  const removeListener = finished(ws, common.mustNotCall());
  checkListeners(ws);
  removeListener();
  checkListeners(ws, 'none');
  ws.end();
}

{
  const rs = new Readable();
  const removeListeners = finished(rs, common.mustNotCall());
  checkListeners(rs);
  removeListeners();
  checkListeners(rs, 'none');

  rs.emit('close');
  rs.push(null);
  rs.resume();
}

{
  const req = Object.assign(new EE(), {
    setHeader() {},
    abort() {}
  });
  const removeListeners = finished(req, common.mustNotCall());
  checkListeners(req, 'legacyrequest');
  removeListeners();
  checkListeners(req, 'none');
}

{
  const streamLike = new EE();
  streamLike.readableEnded = true;
  streamLike.readable = true;
  const removeListeners = finished(streamLike, common.mustCall);
  checkListeners(streamLike);
  removeListeners();
  checkListeners(streamLike, 'none');
}

{
  const streamLike = new EE();
  streamLike.readableEnded = true;
  streamLike.writable = true;
  const removeListeners = finished(streamLike, common.mustCall);
  checkListeners(streamLike, 'legacywritable');
  removeListeners();
  checkListeners(streamLike, 'none');
}

function checkListeners(stream, type) {
  if (type === 'legacyrequest') {
    assert.strictEqual(stream.listenerCount('complete'), 1);
    assert.strictEqual(stream.listenerCount('abort'), 1);
    assert.strictEqual(stream.listenerCount('request'), 1);
    assert.strictEqual(stream.listenerCount('close'), 1);
    assert.strictEqual(stream.listenerCount('finish'), 1);
    assert.strictEqual(stream.listenerCount('end'), 0);
    assert.strictEqual(stream.listenerCount('request'), 1);
  } else if (type === 'legacywritable') {
    assert.strictEqual(stream.listenerCount('complete'), 0);
    assert.strictEqual(stream.listenerCount('abort'), 0);
    assert.strictEqual(stream.listenerCount('request'), 0);
    assert.strictEqual(stream.listenerCount('close'), 2);
    assert.strictEqual(stream.listenerCount('finish'), 1);
    assert.strictEqual(stream.listenerCount('end'), 1);
  } else if (type === 'none') {
    assert.strictEqual(stream.listenerCount('complete'), 0);
    assert.strictEqual(stream.listenerCount('abort'), 0);
    assert.strictEqual(stream.listenerCount('request'), 0);
    assert.strictEqual(stream.listenerCount('close'), 0);
    assert.strictEqual(stream.listenerCount('end'), 0);
    assert.strictEqual(stream.listenerCount('finish'), 0);
  } else {
    assert.strictEqual(stream.listenerCount('complete'), 0);
    assert.strictEqual(stream.listenerCount('abort'), 0);
    assert.strictEqual(stream.listenerCount('request'), 0);
    assert.strictEqual(stream.listenerCount('close'), 1);
    if (stream.writable) {
      assert.strictEqual(stream.listenerCount('finish'), 1);
    }
    if (stream.readable) {
      assert.strictEqual(stream.listenerCount('end'), 1);
    }
  }
}
