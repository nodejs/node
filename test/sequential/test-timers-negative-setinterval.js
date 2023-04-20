'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/negative-setinterval.any.js

let i = 0;
let interval;
function next() {
  i++;
  if (i === 20) {
    clearInterval(interval);
    process.exit();
  }
}
setTimeout(common.mustNotCall(), 1000);
interval = setInterval(next, -100);
