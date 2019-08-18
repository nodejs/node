'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  read.on('close', common.mustCall());

  read.destroy();
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  const expected = new Error('kaboom');

  read.on('end', common.mustNotCall('no end event'));
  read.on('close', common.mustCall());
  read.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  read.destroy(expected);
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });

  read._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, expected);
    cb(err);
  });

  const expected = new Error('kaboom');

  read.on('end', common.mustNotCall('no end event'));
  read.on('close', common.mustCall());
  read.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  read.destroy(expected);
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {},
    destroy: common.mustCall(function(err, cb) {
      assert.strictEqual(err, expected);
      cb();
    })
  });

  const expected = new Error('kaboom');

  read.on('end', common.mustNotCall('no end event'));

  // Error is swallowed by the custom _destroy
  read.on('error', common.mustNotCall('no error event'));
  read.on('close', common.mustCall());

  read.destroy(expected);
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });

  read._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    cb();
  });

  read.destroy();
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  read._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    process.nextTick(() => {
      this.push(null);
      cb();
    });
  });

  const fail = common.mustNotCall('no end event');

  read.on('end', fail);
  read.on('close', common.mustCall());

  read.destroy();

  read.removeListener('end', fail);
  read.on('end', common.mustCall());
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });

  const expected = new Error('kaboom');

  read._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    cb(expected);
  });

  let ticked = false;
  read.on('end', common.mustNotCall('no end event'));
  read.on('error', common.mustCall((err) => {
    assert.strictEqual(ticked, true);
    assert.strictEqual(read._readableState.errorEmitted, true);
    assert.strictEqual(read._readableState.errored, true);
    assert.strictEqual(err, expected);
  }));

  read.destroy();
  assert.strictEqual(read._readableState.errorEmitted, false);
  assert.strictEqual(read._readableState.errored, true);
  assert.strictEqual(read.destroyed, true);
  ticked = true;
}

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  read.destroyed = true;
  assert.strictEqual(read.destroyed, true);

  // The internal destroy() mechanism should not be triggered
  read.on('end', common.mustNotCall());
  read.destroy();
}

{
  function MyReadable() {
    assert.strictEqual(this.destroyed, false);
    this.destroyed = false;
    Readable.call(this);
  }

  Object.setPrototypeOf(MyReadable.prototype, Readable.prototype);
  Object.setPrototypeOf(MyReadable, Readable);

  new MyReadable();
}

{
  // Destroy and destroy callback
  const read = new Readable({
    read() {}
  });
  read.resume();

  const expected = new Error('kaboom');

  let ticked = false;
  read.on('close', common.mustCall(() => {
    assert.strictEqual(read._readableState.errorEmitted, true);
    assert.strictEqual(ticked, true);
  }));
  read.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  assert.strictEqual(read._readableState.errored, false);
  assert.strictEqual(read._readableState.errorEmitted, false);

  read.destroy(expected, common.mustCall(function(err) {
    assert.strictEqual(read._readableState.errored, true);
    assert.strictEqual(err, expected);
  }));
  assert.strictEqual(read._readableState.errorEmitted, false);
  assert.strictEqual(read._readableState.errored, true);
  ticked = true;
}

{
  const readable = new Readable({
    destroy: common.mustCall(function(err, cb) {
      process.nextTick(cb, new Error('kaboom 1'));
    }),
    read() {}
  });

  let ticked = false;
  readable.on('close', common.mustCall(() => {
    assert.strictEqual(ticked, true);
    assert.strictEqual(readable._readableState.errorEmitted, true);
  }));
  readable.on('error', common.mustCall((err) => {
    assert.strictEqual(ticked, true);
    assert.strictEqual(err.message, 'kaboom 1');
    assert.strictEqual(readable._readableState.errorEmitted, true);
  }));

  readable.destroy();
  assert.strictEqual(readable.destroyed, true);
  assert.strictEqual(readable._readableState.errored, false);
  assert.strictEqual(readable._readableState.errorEmitted, false);

  // Test case where `readable.destroy()` is called again with an error before
  // the `_destroy()` callback is called.
  readable.destroy(new Error('kaboom 2'));
  assert.strictEqual(readable._readableState.errorEmitted, false);
  assert.strictEqual(readable._readableState.errored, false);

  ticked = true;
}

{
  const read = new Readable({
    read() {}
  });

  read.destroy();
  read.push('hi');
  read.on('data', common.mustNotCall());
}

{
  const read = new Readable({
    read: common.mustNotCall(function() {})
  });
  read.destroy();
  assert.strictEqual(read.destroyed, true);
  read.read();
}
