'use strict';
const common = require('../common');
const assert = require('assert');

process.on('uncaughtException', function(err) {
  console.log(`Caught exception: ${err}`);
});

setTimeout(common.mustCall(function() {
  console.log('This will still run.');
}), 50);

// Intentionally cause an exception, but don't catch it.
nonexistentFunc(); // eslint-disable-line no-undef
assert.fail('This will not run.');
