'use strict';

// This test makes sure that when throwing from within a timer's callback,
// its active domain at the time of the throw is not the process' active domain
// for the next timers that need to be processed on the same turn of the event
// loop.

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

// Use the same timeout value so that both timers' callbacks are called during
// the same invocation of the underlying native timer's callback (listOnTimeout
// in lib/timers.js).
setTimeout(err, 50);
setTimeout(common.mustCall(secondTimer), 50);

function err() {
  const d = domain.create();
  d.on('error', handleDomainError);
  d.run(err2);

  function err2() {
    // this function doesn't exist, and throws an error as a result.
    err3(); // eslint-disable-line no-undef
  }

  function handleDomainError(e) {
    // In the domain's error handler, the current active domain should be the
    // domain within which the error was thrown.
    assert.equal(process.domain, d);
  }
}

function secondTimer() {
  // secondTimer was scheduled before any domain had been created, so its
  // callback should not have any active domain set when it runs.
  if (process.domain !== null) {
    console.log('process.domain should be null in this timer callback, but is:',
                process.domain);
    // Do not use assert here, as it throws errors and if a domain with an error
    // handler is active, then asserting wouldn't make the test fail.
    process.exit(1);
  }
}
