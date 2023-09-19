'use strict';

const common = require('../common');

// This test checks whether a refresh called inside the callback will keep
// the event loop alive to run the timer again.

let didCall = false;
const timer = setTimeout(common.mustCall(() => {
  if (!didCall) {
    didCall = true;
    timer.refresh();
  }
}, 2), 1);
