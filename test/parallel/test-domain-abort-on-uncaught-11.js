'use strict';

// This test makes sure that when using --abort-on-uncaught-exception and
// when throwing an error from within a domain that has an error handler
// setup, the process _does not_ abort.

const common = require('../common');
const domain = require('domain');

const test = () => {
  const d = domain.create();
  const d2 = domain.create();
  d2.on('error', common.mustCall(() => {}));

  d.run(function() {
    d2.run(function() {
      process.nextTick(function() {
        throw new Error('boom!');
      });
    });
  });
};


if (process.argv[2] === 'child') {
  test();
} else {
  common.childShouldNotThrowAndAbort();
}
