'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Like test-async-hooks-worker-promise.js but with the `await` and `createHook`
// lines switched, because that resulted in different assertion failures
// (one a Node.js assertion and one a V8 DCHECK) and it seems prudent to
// cover both of those failures.

const w = new Worker(`
const { createHook } = require('async_hooks');

setImmediate(async () => {
  await 0;
  createHook({ init() {} }).enable();
  process.exit();
});
`, { eval: true });

w.postMessage({});
w.on('exit', common.mustCall());
