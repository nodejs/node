const {test, afterEach} = require('node:test');
const assert = require('node:assert');
const { waitForAbort } = require('./wait-for-abort-helper');

let testCount = 0;
let signal;

afterEach(() => {
  assert.equal(signal.aborted, false);

  waitForAbort({ testNumber: ++testCount, signal });
});

test("sync", (t) => {
  signal = t.signal;
  assert.equal(signal.aborted, false);
  throw new Error('failing the sync test');
});

test("async", async (t) => {
  await null;
  signal = t.signal;
  assert.equal(signal.aborted, false);
  throw new Error('failing the async test');
});
