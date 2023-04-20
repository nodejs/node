'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/missing-timeout-setinterval.any.js

// Calling setInterval with no interval should be the same as if called with 0 interval
{
  let ctr = 0;
  let doneHandle;
  // eslint-disable-next-line no-restricted-syntax
  const handle = setInterval(() => {
    if (++ctr === 2) {
      clearInterval(handle);
      clearTimeout(doneHandle);
    }
  }/* no interval */);

  doneHandle = setTimeout(common.mustNotCall(), 100);
}

// Calling setInterval with undefined interval should be the same as if called with 0 interval
{
  let ctr = 0;
  let doneHandle;
  const handle = setInterval(() => {
    if (++ctr === 2) {
      clearInterval(handle);
      clearTimeout(doneHandle);
    }
  }, undefined);

  doneHandle = setTimeout(common.mustNotCall(), 100);
}
