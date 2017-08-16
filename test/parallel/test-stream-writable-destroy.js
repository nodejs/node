'use strict';

const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');
const { inherits } = require('util');

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.on('finish', common.mustCall());

  write.destroy();
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  const expected = new Error('kaboom');

  write.on('finish', common.mustCall());
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

  // error is swallowed by the custom _destroy
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

  write.on('error', common.mustCall());

  write.destroy(new Error('kaboom 1'));
  write.destroy(new Error('kaboom 2'));
  assert.strictEqual(write._writableState.errorEmitted, true);
  assert.strictEqual(write.destroyed, true);
}

{
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.destroyed = true;
  assert.strictEqual(write.destroyed, true);

  // the internal destroy() mechanism should not be triggered
  write.on('finish', common.mustNotCall());
  write.destroy();
}

{
  function MyWritable() {
    assert.strictEqual(this.destroyed, false);
    this.destroyed = false;
    Writable.call(this);
  }

  inherits(MyWritable, Writable);

  new MyWritable();
}

{
  // destroy and destroy callback
  const write = new Writable({
    write(chunk, enc, cb) { cb(); }
  });

  write.destroy();

  const expected = new Error('kaboom');

  write.destroy(expected, common.mustCall(function(err) {
    assert.strictEqual(expected, err);
  }));
}
