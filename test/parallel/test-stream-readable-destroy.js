'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');
const { inherits } = require('util');

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  read.on('end', common.mustCall());

  read.destroy();
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  const expected = new Error('kaboom');

  read.on('end', common.mustCall());
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

  // error is swallowed by the custom _destroy
  read.on('error', common.mustNotCall('no error event'));

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

  read.on('end', common.mustNotCall('no end event'));
  read.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  read.destroy();
  assert.strictEqual(read.destroyed, true);
}

{
  const read = new Readable({
    read() {}
  });
  read.resume();

  read.destroyed = true;
  assert.strictEqual(read.destroyed, true);

  // the internal destroy() mechanism should not be triggered
  read.on('end', common.mustNotCall());
  read.destroy();
}

{
  function MyReadable() {
    assert.strictEqual(this.destroyed, false);
    this.destroyed = false;
    Readable.call(this);
  }

  inherits(MyReadable, Readable);

  new MyReadable();
}

{
  // destroy and destroy callback
  const read = new Readable({
    read() {}
  });
  read.resume();

  const expected = new Error('kaboom');

  read.destroy(expected, common.mustCall(function(err) {
    assert.strictEqual(expected, err);
  }));
}
