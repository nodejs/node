'use strict';
const common = require('../common');

process.on('uncaughtException', function(err) {
  console.log('Caught exception: ' + err);
});

setTimeout(common.mustCall(function() {
  console.log('This will still run.');
}), 50);

// Intentionally cause an exception, but don't catch it.
nonexistentFunc(); // eslint-disable-line no-undef
common.fail('This will not run.');
