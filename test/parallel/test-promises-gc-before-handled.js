'use strict';
const common = require('../common');

// Flags: --expose-gc

var p = new Promise((res, rej) => {
  consol.log('oops'); // eslint-disable-line no-undef
});

// Manually call GC due to possible memory contraints with attempting to
// trigger it "naturally".
setTimeout(common.mustCall(() => {
  /* eslint-disable no-undef */
  gc();
  gc();
  gc();
  /* eslint-enable no-undef */
  setTimeout(common.mustCall(() => {
    /* eslint-disable no-undef */
    gc();
    gc();
    gc();
    /* eslint-enable no-undef */
    setTimeout(common.mustCall(() => {
      p.catch(() => {});
    }, 1), 250);
  }), 20);
}), 20);
