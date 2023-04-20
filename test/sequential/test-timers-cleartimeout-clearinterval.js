'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/cleartimeout-cleartinterval.any.js

{
  const handle = setTimeout(common.mustNotCall(), 0);
  clearInterval(handle);
}

{
  const handle = setInterval(common.mustNotCall(), 0);
  clearTimeout(handle);
}

setTimeout(process.exit, 100);
