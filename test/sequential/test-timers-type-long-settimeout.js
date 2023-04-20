'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/type-long-settimeout.any.js

setTimeout(process.exit, Math.pow(2, 32));
setTimeout(common.mustNotCall(), 100);
