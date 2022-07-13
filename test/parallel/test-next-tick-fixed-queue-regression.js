'use strict';

const common = require('../common');

// This tests a highly specific regression tied to the FixedQueue size, which
// was introduced in Node.js 9.7.0: https://github.com/nodejs/node/pull/18617
// More specifically, a nextTick list could potentially end up not fully
// clearing in one run through if exactly 2048 ticks were added after
// microtasks were executed within the nextTick loop.

process.nextTick(() => {
  Promise.resolve(1).then(() => {
    for (let i = 0; i < 2047; i++)
      process.nextTick(common.mustCall());
    const immediate = setImmediate(common.mustNotCall());
    process.nextTick(common.mustCall(() => clearImmediate(immediate)));
  });
});
