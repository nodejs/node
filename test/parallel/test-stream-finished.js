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
  finished(streamLike, common.mustCall);
  streamLike.emit('close');
}


{
  // Test is readable check through readable
  const streamLike = new EE();
  streamLike.readable = false;
  finished(streamLike, common.mustCall());
  streamLike.emit('end');
}

{
  // Test is readable check through readableEnded
  const streamLike = new EE();
  streamLike.readableEnded = true;
  finished(streamLike, common.mustCall());
  streamLike.emit('end');
}

{
  // Test is readable check through _readableState
  const streamLike = new EE();
  streamLike._readableState = {};
  finished(streamLike, common.mustCall());
  streamLike.emit('end');
}

{
  // Test is writable check through writable
  const streamLike = new EE();
  streamLike.writable = false;
  finished(streamLike, common.mustCall());
  streamLike.emit('finish');
}

{
  // Test is writable check through writableEnded
  const streamLike = new EE();
  streamLike.writableEnded = true;
  finished(streamLike, common.mustCall());
  streamLike.emit('finish');
}

{
  // Test is writable check through _writableState
  const streamLike = new EE();
  streamLike._writableState = {};
  finished(streamLike, common.mustCall());
  streamLike.emit('finish');
}
