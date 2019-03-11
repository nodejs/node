// Flags: --unhandled-rejections=strict
'use strict';

require('../common');

// Check that the process will exit on the first unhandled rejection in case the
// unhandled rejections mode is set to `'strict'`.

const ref1 = new Promise(() => {
  throw new Error('One');
});

const ref2 = Promise.reject(new Error('Two'));

// Keep the event loop alive to actually detect the unhandled rejection.
setTimeout(() => console.log(ref1, ref2), 1000);
