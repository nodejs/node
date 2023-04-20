'use strict';

const common = require('../common');

// This is a port of https://github.com/web-platform-tests/wpt/blob/22ecfc9/html/webappapis/timers/negative-settimeout.any.js

setTimeout(process.exit, -100);
setTimeout(common.mustNotCall(), 10);
