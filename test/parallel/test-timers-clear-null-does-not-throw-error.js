'use strict';
require('../common');

// This test makes sure clearing timers with
// 'null' or no input does not throw error
clearInterval(null);
clearInterval();
clearTimeout(null);
clearTimeout();
clearImmediate(null);
clearImmediate();
