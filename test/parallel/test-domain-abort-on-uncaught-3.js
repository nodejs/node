'use strict';

// This test makes sure that when using --abort-on-uncaught-exception and
// when throwing an error from within a domain that has an error handler
// setup, the process _does not_ abort.

const common = require('../common');
const domain = require('domain');

const test = () => {
  const d = domain.create();
  d.once('error', common.mustCall(() => { }));

  d.run(function() {
    setTimeout(function() {
      process.nextTick(function() {
        throw new Error('exceptional!');
      });
    }, 33);
  });
};


if (process.argv[2] === 'child') {
  test();
} else {
  common.childShouldNotThrowAndAbort();
}
