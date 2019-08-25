'use strict';
const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

// Ensure write cannot be spammed with pending
// requests.

{
  // Sync
  const w = new Writable({
    write: common.mustCallAtLeast((buf, enc, cb) => {
      process.nextTick(cb);
    }, 1)
  });

  let cnt = 0;
  while (w.write('a'))
    cnt++;

  assert.strictEqual(cnt, w.writablePendingWaterMark - 1);
}

{
  // Async
  const w = new Writable({
    write: common.mustCallAtLeast((buf, enc, cb) => {
      cb();
    }, 1)
  });

  let cnt = 0;
  while (w.write('a'))
    cnt++;

  assert.strictEqual(cnt, w.writablePendingWaterMark - 1);
}
