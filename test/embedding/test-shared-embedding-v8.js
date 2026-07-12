'use strict';

// This tests the V8 parts in the shared library work correctly.
// TODO(joyeecheung): also test that the Node.js parts work correctly,
// which can be done in embedtest just built in shared library mode.

const common = require('../common');

if (!common.usesSharedLibrary) {
  common.skip('Only tests builds linking against Node.js shared library');
}

const { spawnSyncAndAssert } = require('../common/child_process');
const fs = require('fs');

const binary = common.resolveBuiltBinary('shared_embedtest');

if (!fs.existsSync(binary)) {
  common.skip('shared_embedtest binary not built');
}

spawnSyncAndAssert(binary, {
  trim: true,
  stdout: 'Hello, World!',
});
