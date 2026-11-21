'use strict';

require('../common');

const assert = require('assert');
const os = require('os');

// -0 passes the int32 validation, so it must be accepted like 0 instead of
// aborting the process in the binding.
assert.strictEqual(os.getPriority(-0), os.getPriority(0));

os.setPriority(-0, os.getPriority());

// Setting the priority to 0 may need more privileges than the process has,
// which is reported as a system error rather than an abort.
for (const args of [[0, -0], [-0, -0], [-0]]) {
  try {
    os.setPriority(...args);
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_SYSTEM_ERROR');
    assert.strictEqual(err.info.code, 'EACCES');
  }
}
