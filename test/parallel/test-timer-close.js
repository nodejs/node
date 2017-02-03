'use strict';
const common = require('../common');

// Make sure handle._handle.close(callback) is idempotent by closing a timer
// twice. The first function should be called, the second one should not.

const Timer = process.binding('timer_wrap').Timer;
const t = new Timer();

t.close(common.mustCall(function() {}));
t.close(common.mustNotCall());
