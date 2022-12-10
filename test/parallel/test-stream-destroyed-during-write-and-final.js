'use strict';

const common = require('../common');
const { Writable, addAbortSignal } = require('stream');
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
  
    assert.strictEqual(write.destroyed, true);
}

{
    const s = new Writable({
      final() {}
    });
    const _err = new Error('bad');
    s.end(common.mustCall((err) => {
      assert.strictEqual(err, _err);
    }));
    s.on('error', common.mustCall((err) => {
      assert.strictEqual(err, _err);
    }));
    s.destroy(_err);
}