'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { safeGetenv } = internalBinding('credentials');

// FIXME(joyeecheung): this test is not entirely useful. To properly
// test this we could create a mismatch between the effective/real
// group/user id of a Node.js process and see if the environment variables
// are no longer available - but that might be tricky to set up reliably.

for (const oneEnv in process.env) {
  assert.strictEqual(
    safeGetenv(oneEnv),
    process.env[oneEnv]
  );
}
