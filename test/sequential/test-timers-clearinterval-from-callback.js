'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/clearinterval-from-callback.any.js

let wasPreviouslyCalled = false;

const handle = setInterval(() => {
  if (!wasPreviouslyCalled) {
    wasPreviouslyCalled = true;

    clearInterval(handle);

    setInterval(process.exit, 750);
  } else {
    common.mustNotCall()();
  }
}, 500);
