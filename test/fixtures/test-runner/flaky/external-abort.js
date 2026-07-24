'use strict';
const { test } = require('node:test');
const { setTimeout } = require('node:timers/promises');
const fs = require('node:fs');
const stateFile = process.env.FLAKY_STATE;
const ac = new AbortController();
test('aborted externally not retried', { flaky: 5, signal: ac.signal }, async () => {
  fs.appendFileSync(stateFile, 'x');
  ac.abort(); // user-driven cancellation
  await setTimeout(10000, undefined, { signal: ac.signal });
});
