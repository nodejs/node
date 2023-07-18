const {test, afterEach} = require('node:test');
const assert = require('node:assert');

let testCount = 0;
let signal;

afterEach(() => {
  testCount++;
  assert.equal(signal.aborted, true);

  console.log(`abort called for test ${testCount}`)
});

test("sync", (t) => {
  signal = t.signal;
  assert.equal(signal.aborted, false);
});

test("async", async (t) => {
  await null;
  signal = t.signal;
  assert.equal(signal.aborted, false);
});