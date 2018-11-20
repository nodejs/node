'use strict';

const common = require('../common');
const assert = require('assert');
const _module = require('module'); // avoid collision with global.module

// Current directory gets highest priority for local modules
function testFirstInPath(moduleName, isLocalModule) {
  const assertFunction = isLocalModule ?
    assert.strictEqual :
    assert.notStrictEqual;

  const lookupResults = _module._resolveLookupPaths(moduleName);

  let paths = lookupResults[1];
  assertFunction(paths[0], '.');

  paths = _module._resolveLookupPaths(moduleName, null, true);
  assertFunction(paths && paths[0], '.');
}

testFirstInPath('./lodash', true);

// Relative path on Windows, but a regular file name elsewhere
testFirstInPath('.\\lodash', common.isWindows);
