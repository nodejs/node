'use strict';
const common = require('../common');

const timeout = setTimeout(common.mustCall(), 10);
timeout.unref();

// Wrap `close` method to check if the handle was closed
const close = timeout._handle.close;
timeout._handle.close = common.mustCall(function() {
  return close.apply(this, arguments);
});

// Just to keep process alive and let previous timer's handle die
setTimeout(() => {}, 50);
