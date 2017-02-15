'use strict';
// Flags: --no-warnings

// The --no-warnings flag only supresses writing the warning to stderr, not the
// emission of the corresponding event. This test file can be run without it.

const common = require('../common');
process.noDeprecation = true;

const assert = require('assert');

function listener() {
  common.fail('received unexpected warning');
}

process.addListener('warning', listener);

process.emitWarning('Something is deprecated.', 'DeprecationWarning');

// The warning would be emitted in the next tick, so continue after that.
process.nextTick(common.mustCall(() => {
  // Check that deprecations can be re-enabled.
  process.noDeprecation = false;
  process.removeListener('warning', listener);

  process.addListener('warning', common.mustCall((warning) => {
    assert.strictEqual(warning.name, 'DeprecationWarning');
    assert.strictEqual(warning.message, 'Something else is deprecated.');
  }));

  process.emitWarning('Something else is deprecated.', 'DeprecationWarning');
}));
