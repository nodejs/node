// Flags: --no-warnings
'use strict';

const common = require('../common');

const { Dir } = require('fs');

const DeprecationWarning = [];
DeprecationWarning.push([
  'Dir.prototype.processReadResult is deprecated.',
  'DEP0198']);
DeprecationWarning.push([
  'Dir.prototype.readSyncRecursive is deprecated.',
  'DEP0198']);

common.expectWarning({ DeprecationWarning });

const dir = new Dir({});

// Verify that the expected deprecation warnings are emitted.

try {
  dir.processReadResult('/', []);
} catch {
  // We expect these to throw. We don't care about the error.
}

try {
  dir.readSyncRecursive({ path: '/' });
} catch {
  // We expect these to throw. We don't care about the error.
}
