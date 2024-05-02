'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Like test-async-hooks-worker-promise.js but with an additional statement
// after the `process.exit()` call, that shouldn’t really make a difference
// but apparently does.

const w = new Worker(`
const { createHook } = require('async_hooks');

setImmediate(async () => {
  createHook({ init() {} }).enable();
  await 0;
  process.exit();
  process._rawDebug('THIS SHOULD NEVER BE REACHED');
});
`, { eval: true });

w.on('exit', common.mustCall());
