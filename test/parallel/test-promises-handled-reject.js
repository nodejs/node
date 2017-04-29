'use strict';
const common = require('../common');

// Flags: --expose-gc

const p = new Promise((res, rej) => {
  consol.log('oops'); // eslint-disable-line no-undef
});

// Manually call GC due to possible memory contraints with attempting to
// trigger it "naturally".
process.nextTick(common.mustCall(() => {
  p.catch(() => {});
  /* eslint-disable no-undef */
  gc();
  gc();
  gc();
  /* eslint-enable no-undef */
}, 1));
