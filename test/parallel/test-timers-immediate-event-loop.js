// Flags: --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');
const { sleep } = require('internal/util');

// SetImmediate should clear its existing queue on each event loop run
// but leave any newly scheduled Immediates until the next run.
//
// Since timer expiries are notoriously flaky and dependant on specific
// OS configurations, we instead just check that an Immediate queue
// will eventually be interrupted by the timer — regardless of how long
// it actually takes for that to happen.

const duration = 1;
let nextImmediate;

const exit = common.mustCall(() => {
  assert.ok(nextImmediate.hasRef());
  process.exit();
});

function check() {
  sleep(duration);
  nextImmediate = setImmediate(check);
}

setImmediate(common.mustCall(() => {
  setTimeout(exit, duration);

  check();
}));
