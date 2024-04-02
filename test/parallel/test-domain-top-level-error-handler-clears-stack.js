'use strict';

const common = require('../common');
const domain = require('domain');

// Make sure that the domains stack is cleared after a top-level domain
// error handler exited gracefully.
const d = domain.create();

d.on('error', common.mustCall(() => {
  // Scheduling a callback with process.nextTick _could_ enter a _new_ domain,
  // but domain's error handlers are called outside of their domain's context.
  // So there should _no_ domain on the domains stack if the domains stack was
  // cleared properly when the domain error handler was called.
  process.nextTick(() => {
    if (domain._stack.length !== 0) {
      // Do not use assert to perform this test: this callback runs in a
      // different callstack as the original process._fatalException that
      // handled the original error, thus throwing here would trigger another
      // call to process._fatalException, and so on recursively and
      // indefinitely.
      console.error('domains stack length should be 0, but instead is:',
                    domain._stack.length);
      process.exit(1);
    }
  });
}));

d.run(() => {
  throw new Error('Error from domain');
});
