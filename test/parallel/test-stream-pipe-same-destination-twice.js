'use strict';
const common = require('../common');

// Regression test for https://github.com/nodejs/node/issues/12718.
// Tests that piping a source stream twice to the same destination stream
// works, and that a subsequent unpipe() call only removes the pipe *once*.
const assert = require('assert');
const { PassThrough, Writable } = require('stream');

{
  const passThrough = new PassThrough();
  const dest = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      assert.strictEqual(`${chunk}`, 'foobar');
      cb();
    })
  });

  passThrough.pipe(dest);
  passThrough.pipe(dest);

  assert.strictEqual(passThrough._events.data.length, 2);
  assert.strictEqual(passThrough._readableState.pipes.length, 2);
  assert.strictEqual(passThrough._readableState.pipes[0], dest);
  assert.strictEqual(passThrough._readableState.pipes[1], dest);

  passThrough.unpipe(dest);

  assert.strictEqual(passThrough._events.data.length, 1);
  assert.strictEqual(passThrough._readableState.pipes.length, 1);
  assert.deepStrictEqual(passThrough._readableState.pipes, [dest]);

  passThrough.write('foobar');
  passThrough.pipe(dest);
}

{
  const passThrough = new PassThrough();
  const dest = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      assert.strictEqual(`${chunk}`, 'foobar');
      cb();
    }, 2)
  });

  passThrough.pipe(dest);
  passThrough.pipe(dest);

  assert.strictEqual(passThrough._events.data.length, 2);
  assert.strictEqual(passThrough._readableState.pipes.length, 2);
  assert.strictEqual(passThrough._readableState.pipes[0], dest);
  assert.strictEqual(passThrough._readableState.pipes[1], dest);

  passThrough.write('foobar');
}

{
  const passThrough = new PassThrough();
  const dest = new Writable({
    write: common.mustNotCall()
  });

  passThrough.pipe(dest);
  passThrough.pipe(dest);

  assert.strictEqual(passThrough._events.data.length, 2);
  assert.strictEqual(passThrough._readableState.pipes.length, 2);
  assert.strictEqual(passThrough._readableState.pipes[0], dest);
  assert.strictEqual(passThrough._readableState.pipes[1], dest);

  passThrough.unpipe(dest);
  passThrough.unpipe(dest);

  assert.strictEqual(passThrough._events.data, undefined);
  assert.strictEqual(passThrough._readableState.pipes.length, 0);

  passThrough.write('foobar');
}
