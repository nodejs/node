'use strict';

// This test makes sure that when using --abort-on-uncaught-exception and
// when throwing an error from within a domain that has an error handler
// setup, the process _does not_ abort.

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const child_process = require('child_process');

let errorHandlerCalled = false;

let test = () => {
  const d = domain.create();
  const d2 = domain.create();

  d2.on('error', function errorHandler() {
    errorHandlerCalled = true;
  });

  d.run(function() {
    d2.run(function() {
      setTimeout(function() {
        console.log('foo');
        throw new Error('boom!');
      }, 33);
    });
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
