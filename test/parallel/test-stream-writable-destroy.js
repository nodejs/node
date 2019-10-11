'use strict';

const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.on('finish', common.mustNotCall());
  write.on('close', common.mustCall());

  write.destroy();
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) {
      this.destroy(new Error('asd'));
      cb();
    }
  });

  write.on('error', common.mustCall());
  write.on('finish', common.mustNotCall());
  write.end('asd');
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  const expected = new Error('kaboom');

  write.on('finish', common.mustNotCall());
  write.on('close', common.mustCall());
  write.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  write.destroy(expected);
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write._destroy = function(err, cb) {
    assert.strictEqual(err, expected);
    cb(err);
  };

  const expected = new Error('kaboom');

  write.on('finish', common.mustNotCall('no finish event'));
  write.on('close', common.mustCall());
  write.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  write.destroy(expected);
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); },
    destroy: common.mustCall(function(err, cb) {
      assert.strictEqual(err, expected);
      cb();
    })
  });

  const expected = new Error('kaboom');

  write.on('finish', common.mustNotCall('no finish event'));
  write.on('close', common.mustCall());

  // Error is swallowed by the custom _destroy
  write.on('error', common.mustNotCall('no error event'));

  write.destroy(expected);
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    cb();
  });

  write.destroy();
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    process.nextTick(() => {
      this.end();
      cb();
    });
  });

  const fail = common.mustNotCall('no finish event');

  write.on('finish', fail);
  write.on('close', common.mustCall());

  write.destroy();

  write.removeListener('finish', fail);
  write.on('finish', common.mustCall());
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  const expected = new Error('kaboom');

  write._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    cb(expected);
  });

  write.on('close', common.mustCall());
  write.on('finish', common.mustNotCall('no finish event'));
  write.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  write.destroy();
  assert.strictEqual(write.destroyed, true);
}

{
  // double error case
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.on('close', common.mustCall());
  write.on('error', common.mustCall());

  write.destroy(new Error('kaboom 1'));
  write.destroy(new Error('kaboom 2'));
  assert.strictEqual(write._writableState.errorEmitted, true);
  assert.strictEqual(write.destroyed, true);
}

{
  const writable = new Writable({
    destroy: common.mustCall(function(err, cb) {
      process.nextTick(cb, new Error('kaboom 1'));
    }),
    write(chunk, enc, cb) {
      cb();
    }
  });

  writable.on('close', common.mustCall());
  writable.on('error', common.expectsError({
    type: Error,
    message: 'kaboom 2'
  }));

  writable.destroy();
  assert.strictEqual(writable.destroyed, true);
  assert.strictEqual(writable._writableState.errorEmitted, false);

  // Test case where `writable.destroy()` is called again with an error before
  // the `_destroy()` callback is called.
  writable.destroy(new Error('kaboom 2'));
  assert.strictEqual(writable._writableState.errorEmitted, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.destroyed = true;
  assert.strictEqual(write.destroyed, true);

  // The internal destroy() mechanism should not be triggered
  write.on('close', common.mustNotCall());
  write.destroy();
}

{
  function MyWritable() {
    assert.strictEqual(this.destroyed, false);
    this.destroyed = false;
    Writable.call(this);
  }

  Object.setPrototypeOf(MyWritable.prototype, Writable.prototype);
  Object.setPrototypeOf(MyWritable, Writable);

  new MyWritable();
}

{
  // Destroy and destroy callback
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.destroy();

  const expected = new Error('kaboom');

  write.destroy(expected, common.mustCall(function(err) {
    assert.strictEqual(err, expected);
  }));
}

{
  // Checks that `._undestroy()` restores the state so that `final` will be
  // called again.
  const write = new Writable({
    write: common.mustNotCall(),
    final: common.mustCall((cb) => cb(), 2)
  });

  write.end();
  write.destroy();
  write._undestroy();
  write.end();
}

{
  const write = new Writable();

  write.destroy();
  write.on('error', common.expectsError({
    type: Error,
    code: 'ERR_STREAM_DESTROYED',
    message: 'Cannot call write after a stream was destroyed'
  }));
  write.write('asd', common.expectsError({
    type: Error,
    code: 'ERR_STREAM_DESTROYED',
    message: 'Cannot call write after a stream was destroyed'
  }));
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.on('error', common.expectsError({
    type: Error,
    code: 'ERR_STREAM_DESTROYED',
    message: 'Cannot call write after a stream was destroyed'
  }));

  write.cork();
  write.write('asd', common.mustCall());
  write.uncork();

  write.cork();
  write.write('asd', common.expectsError({
    type: Error,
    code: 'ERR_STREAM_DESTROYED',
    message: 'Cannot call write after a stream was destroyed'
  }));
  write.destroy();
  write.write('asd', common.expectsError({
    type: Error,
    code: 'ERR_STREAM_DESTROYED',
    message: 'Cannot call write after a stream was destroyed'
  }));
  write.uncork();
}

{
  // Call end(cb) after error & destroy

  const write = new Writable({
    write(chunk, enc, cb) { cb(new Error('asd')); }
  });
  write.on('error', common.mustCall(() => {
    write.destroy();
    let ticked = false;
    write.end(common.mustCall((err) => {
      assert.strictEqual(ticked, true);
      assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
    }));
    ticked = true;
  }));
  write.write('asd');
}

{
  // Call end(cb) after finish & destroy

  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });
  write.on('finish', common.mustCall(() => {
    write.destroy();
    let ticked = false;
    write.end(common.mustCall((err) => {
      assert.strictEqual(ticked, false);
      assert.strictEqual(err.code, 'ERR_STREAM_ALREADY_FINISHED');
    }));
    ticked = true;
  }));
  write.end();
}

{
  // Call end(cb) after error & destroy and don't trigger
  // unhandled exception.

  const write = new Writable({
    write(chunk, enc, cb) { process.nextTick(cb); }
  });
  write.once('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'asd');
  }));
  write.end('asd', common.mustCall((err) => {
    assert.strictEqual(err.message, 'asd');
  }));
  write.destroy(new Error('asd'));
}
