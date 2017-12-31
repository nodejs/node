'use strict';

const common = require('../common');
const assert = require('assert');

let unref_interval = false;
let unref_timer = false;
let checks = 0;

const LONG_TIME = 10 * 1000;
const SHORT_TIME = 100;

assert.doesNotThrow(() => {
  setTimeout(() => {}, 10).unref().ref().unref();
}, 'ref and unref are chainable');

assert.doesNotThrow(() => {
  setInterval(() => {}, 10).unref().ref().unref();
}, 'ref and unref are chainable');

setInterval(common.mustNotCall('Interval should not fire'), LONG_TIME).unref();
setTimeout(common.mustNotCall('Timer should not fire'), LONG_TIME).unref();

const interval = setInterval(common.mustCall(() => {
  unref_interval = true;
  clearInterval(interval);
}), SHORT_TIME);
interval.unref();

setTimeout(common.mustCall(() => {
  unref_timer = true;
}), SHORT_TIME).unref();

const check_unref = setInterval(() => {
  if (checks > 5 || (unref_interval && unref_timer))
    clearInterval(check_unref);
  checks += 1;
}, 100);

{
  const timeout =
    setTimeout(common.mustCall(() => {
      timeout.unref();
    }), SHORT_TIME);
}

{
  // Should not timeout the test
  const timeout =
    setInterval(() => timeout.unref(), SHORT_TIME);
}

// Should not assert on args.Holder()->InternalFieldCount() > 0.
// See https://github.com/nodejs/node-v0.x-archive/issues/4261.
{
  const t = setInterval(() => {}, 1);
  process.nextTick(t.unref.bind({}));
  process.nextTick(t.unref.bind(t));
}
