'use strict';

// This test makes sure that when using --abort-on-uncaught-exception and
// when throwing an error from within a domain that has an error handler
// setup, the process _does not_ abort.

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

let errorHandlerCalled = false;

let test = () => {
  const d = domain.create();

  d.once('error', (err) => {
    errorHandlerCalled = true;
  });

  d.run(function() {
    throw new Error('exceptional!');
  });
};


if (process.argv[2] === 'child') {
  test();
  process.on('exit', function onExit() {
    assert.strictEqual(errorHandlerCalled, true);
  });
} else {
  common.childShouldNotThrowAndAbort();
}
