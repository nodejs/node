// Flags: --expose-gc
'use strict';

const common = require('../common');

process.env.NODE_UNHANDLED_REJECTION = 'SILENT';

// Verify that setting the environment variable in a handler will this change
// the behavior properly.

Promise.reject('test');

new Promise(() => {
  throw new Error('One');
});

global.gc();

process.on('unhandledRejection', () => {
  process.env.NODE_UNHANDLED_REJECTION = 'ERROR';
});
process.on('uncaughtException', common.mustCall((err) => {
  common.expectsError({
    message: 'One'
  })(err);
}));

setTimeout(common.mustCall(), 2);
