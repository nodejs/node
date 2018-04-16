// Flags: --expose-gc
'use strict';

const common = require('../common');
// TODO: Remove when making this the default.
process.env.NODE_UNHANDLED_REJECTION = 'ERROR_ON_GC';

// Verify that unhandled rejections cause `process.exit` to be called with exit
// code 1 if they get garbage collected. Any promise that is handled later on
// should not cause this.

let p1 = new Promise((res, rej) => {
  throw new Error('One');
});

// eslint-disable-next-line no-unused-vars
let p2 = new Promise((res, rej) => {
  throw new Error('Two');
});

const p3 = Promise.reject(new Error('Three'));

global.gc();

process.nextTick(() => {
  console.log('First GC did not collect it.');
  global.gc();
  // Should not trigger an error.
  p1.catch(() => {});
  p1 = null;
  setImmediate(() => {
    console.log('Second GC did not collect it.');
    setTimeout(() => {
      // Should still trigger a warning.
      p3.catch(() => {});
    }, 1000);
    global.gc();
    setImmediate(() => {
      console.log('Third GC did not collect it.');
      // Free the reference and exit on GC
      p2 = null;
      // Trigger a fatal exception on setImmediate
      global.gc();
      setImmediate(() => {
        console.log('Should not be reached');
      });
    });
  });
});

process.on('unhandledRejection', common.mustCall(console.log, 2));

process.on('exit', (code) => process._rawDebug(`Exit code ${code}`));
