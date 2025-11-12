'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');

const hooks = initHooks();
hooks.enable();

setImmediate(() => {
  throw new Error();
});

setTimeout(() => {
  throw new Error();
}, 1);

process.on('uncaughtException', common.mustCall(2));
