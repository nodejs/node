'use strict';

const common = require('../common');
const timers = require('timers');

{
  const interval = setInterval(common.mustCall(() => {
    clearTimeout(interval);
  }), 1).unref();
}

{
  const interval = setInterval(common.mustCall(() => {
    interval.close();
  }), 1).unref();
}

{
  const interval = setInterval(common.mustCall(() => {
    timers.unenroll(interval);
  }), 1).unref();
}

{
  const interval = setInterval(common.mustCall(() => {
    interval._idleTimeout = -1;
  }), 1).unref();
}

{
  const interval = setInterval(common.mustCall(() => {
    interval._onTimeout = null;
  }), 1).unref();
}

// Use timers' intrinsic behavior to keep this open
// exactly long enough for the problem to manifest.
//
// See https://github.com/nodejs/node/issues/9561
//
// Since this is added after it will always fire later
// than the previous timeouts, unrefed or not.
//
// Keep the event loop alive for one timeout and then
// another. Any problems will occur when the second
// should be called but before it is able to be.
setTimeout(common.mustCall(() => {
  setTimeout(common.mustCall(() => {}), 1);
}), 1);
