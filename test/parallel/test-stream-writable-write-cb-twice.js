'use strict';
const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

{
  // Sync + Sync
  const writable = new Writable({
    write: common.mustCall((buf, enc, cb) => {
      cb();
      cb();
    })
  });
  writable.write('hi');
  writable.on('error', common.expectsError({
    code: 'ERR_MULTIPLE_CALLBACK',
    type: Error
  }));
}

{
  // Sync + Async
  const writable = new Writable({
    write: common.mustCall((buf, enc, cb) => {
      cb();
      process.nextTick(() => {
        cb();
      });
    })
  });
  writable.write('hi');
  writable.on('error', common.expectsError({
    code: 'ERR_MULTIPLE_CALLBACK',
    type: Error
  }));
}

{
  // Async + Async
  const writable = new Writable({
    write: common.mustCall((buf, enc, cb) => {
      process.nextTick(cb);
      process.nextTick(() => {
        cb();
      });
    })
  });
  writable.write('hi');
  writable.on('error', common.expectsError({
    code: 'ERR_MULTIPLE_CALLBACK',
    type: Error
  }));
}
