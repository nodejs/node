'use strict';
const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

{
  // Sync + Sync
  const writable = new Writable({
    write: common.mustCall((buf, enc, cb) => {
      cb();
      assert.throws(cb, {
        code: 'ERR_MULTIPLE_CALLBACK',
        name: 'Error'
      });
    })
  });
  writable.write('hi');
}

{
  // Sync + Async
  const writable = new Writable({
    write: common.mustCall((buf, enc, cb) => {
      cb();
      process.nextTick(() => {
        assert.throws(cb, {
          code: 'ERR_MULTIPLE_CALLBACK',
          name: 'Error'
        });
      });
    })
  });
  writable.write('hi');
}

{
  // Async + Async
  const writable = new Writable({
    write: common.mustCall((buf, enc, cb) => {
      process.nextTick(cb);
      process.nextTick(() => {
        assert.throws(cb, {
          code: 'ERR_MULTIPLE_CALLBACK',
          name: 'Error'
        });
      });
    })
  });
  writable.write('hi');
}
