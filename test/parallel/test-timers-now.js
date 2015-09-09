'use strict';
// Flags: --expose_internals
const common = require('../common');
const assert = require('assert');

// Return value of Timer.now() should easily fit in a SMI right after start-up.
const Timer = require('binding/timer_wrap').Timer;
assert(Timer.now() < 0x3ffffff);
