'use strict';

// Test that allocating a timer does not increase the loop's reference
// count.

var Timer = process.binding('timer_wrap').Timer;
new Timer();
