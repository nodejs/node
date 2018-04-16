'use strict';

// Currently we keep a reference of unhandled rejections in JS and that prevents
// the GC to collect these before the process exits naturally.

const common = require('../common');
// TODO: Remove when making this the default.
process.env.NODE_UNHANDLED_REJECTION = 'ERROR_ON_GC';

new Promise(function(res, rej) {
  throw new Error('One');
});

Promise.reject(new Error('Two'));

process.on('uncaughtException', common.mustNotCall());

process.on('exit', (code) => process._rawDebug(`Exit code ${code}`));
