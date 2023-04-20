'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/type-long-setinterval.any.js

setInterval(process.exit, Math.pow(2, 32));
setTimeout(common.mustNotCall(), 100);
