'use strict';

const common = require('../common');

const {
  PerformanceObserver,
  performance: {
    timerify,
  },
} = require('perf_hooks');

const assert = require('assert');

const {
  setTimeout: sleep
} = require('timers/promises');

let check = false;

async function doIt() {
  await sleep(100);
  check = true;
  return check;
}

const obs = new PerformanceObserver(common.mustCall((list) => {
  const entry = list.getEntries()[0];
  assert.strictEqual(entry.name, 'doIt');
  assert(check);
  obs.disconnect();
}));

obs.observe({ type: 'function' });

const timerified = timerify(doIt);

const res = timerified();
assert(res instanceof Promise);
res.then(common.mustCall(assert));
