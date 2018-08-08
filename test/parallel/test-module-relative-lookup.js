'use strict';

const common = require('../common');
const assert = require('assert');
const _module = require('module'); // avoid collision with global.module

function runTest(moduleName) {
  const lookupResults = _module._resolveLookupPaths(moduleName);
  let paths = lookupResults[1];

  // Current directory gets highest priority for local modules
  assert.strictEqual(paths[0], '.');

  paths = _module._resolveLookupPaths(moduleName, null, true);

  // Current directory gets highest priority for local modules
  assert.strictEqual(paths && paths[0], '.');
}

runTest('./lodash');
if (common.isWindows) {
  runTest('.\\lodash');
}
