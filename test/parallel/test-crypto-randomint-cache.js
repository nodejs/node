// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Synchronous randomInt() calls made while an asynchronous cache refill is
// pending must not cause the same random bytes to be returned twice.

const assert = require('assert');
const { randomInt } = require('crypto');
const { internalBinding } = require('internal/test/binding');
const { RandomBytesJob } = internalBinding('crypto');

// With this range, every 6-byte draw except 0xffffffffffff is returned
// unchanged, so a repeated value means repeated cache bytes.
const max = 2 ** 48 - 1;
const values = [];

// The refill job calls ondone on the main thread after the threadpool has
// written its buffer. Make the synchronous calls at that point, before the
// refill callback in randomInt() runs.
const { run } = RandomBytesJob.prototype;
RandomBytesJob.prototype.run = common.mustCall(function() {
  RandomBytesJob.prototype.run = run;
  const { ondone } = this;
  this.ondone = common.mustCall((...args) => {
    for (let i = 0; i < 3; i++)
      values.push(randomInt(max));
    ondone(...args);
  });
  run.call(this);
});

// This is the first randomInt() call in the process, so the cache is empty.
// The call is queued and an asynchronous refill starts.
randomInt(max, common.mustSucceed((n) => {
  values.push(n);
  for (let i = 0; i < 3; i++)
    values.push(randomInt(max));
  assert.strictEqual(new Set(values).size, values.length,
                     `duplicate values: ${values}`);
}));
