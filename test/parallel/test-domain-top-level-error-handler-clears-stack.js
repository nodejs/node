'use strict';

const common = require('../common');
const domain = require('domain');

/*
 * Make sure that the domains stack is cleared after a top-level domain
 * error handler exited gracefully.
 */
const d = domain.create();

d.on('error', common.mustCall(function() {
  process.nextTick(function() {
    // Scheduling a callback with process.nextTick will enter a _new_ domain,
    // and the callback will be called after the domain that handled the error
    // was exited. So there should be only one domain on the domains stack if
    // the domains stack was cleared properly when the domain error handler
    // returned.
    if (domain._stack.length !== 1) {
      // Do not use assert to perform this test: this callback runs in a
      // different callstack as the original process._fatalException that
      // handled the original error, thus throwing here would trigger another
      // call to process._fatalException, and so on recursively and
      // indefinitely.
      console.error('domains stack length should be 1, but instead is:',
                    domain._stack.length);
      process.exit(1);
    }
  });
}));

d.run(function() {
  throw new Error('Error from domain');
});
