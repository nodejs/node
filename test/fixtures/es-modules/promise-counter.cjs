// Counts PROMISE async resources created during the process lifetime.
// Used by test-esm-sync-entry-point.mjs to verify the sync ESM loader
// path does not create unnecessary promises.
'use strict';
let count = 0;
require('async_hooks').createHook({
  init(id, type) { if (type === 'PROMISE') count++; },
}).enable();
process.on('exit', () => {
  process.stdout.write(`PROMISE_COUNT=${count}\n`);
});
