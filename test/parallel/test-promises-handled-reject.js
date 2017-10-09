// Flags: --expose-gc

'use strict';
const common = require('../common');

const p = new Promise((res, rej) => {
  throw new Error('oops');
});

// Manually call GC due to possible memory constraints with attempting to
// trigger it "naturally".
setTimeout(common.mustCall(() => {
  p.catch(() => {});
  global.gc();
  global.gc();
  global.gc();
}, 1), 2);
