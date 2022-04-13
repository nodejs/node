'use strict';
require('../common');

// This test makes sure clearing timers with
// objects doesn't throw
clearImmediate({});
clearTimeout({});
clearInterval({});
