// Flags: --expose-gc
'use strict';

const common = require('../common');
// TODO: Remove when making this the default.
process.env.NODE_UNHANDLED_REJECTION = 'ERROR_ON_GC';

new Promise(function(res, rej) {
  throw new Error('Fail');
});

setTimeout(() => {
  global.gc();
}, 1);

process.on('uncaughtException', common.mustCall((err, fromPromise) => {
  console.log(`Caught ${err.message}; From promise: ${fromPromise}`);
}));

process.on('exit', (code) => process._rawDebug(`Exit code ${code}`));
