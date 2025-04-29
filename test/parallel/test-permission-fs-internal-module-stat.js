// Flags: --expose-internals --permission --allow-fs-read=test/common* --allow-fs-read=tools* --allow-fs-read=test/parallel* --allow-child-process --allow-natives-syntax
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');
const { strictEqual } = require('assert');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const { internalBinding } = require('internal/test/binding');
const fixtures = require('../common/fixtures');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const internalFsBinding = internalBinding('fs');

strictEqual(internalFsBinding.internalModuleStat(blockedFile), 0);

// Only javascript methods can be optimized through %OptimizeFunctionOnNextCall
// This is why we surround the C++ method we want to optimize with a JS function.
function testFastPaths(file) {
  return internalFsBinding.internalModuleStat(file);
}

eval('%PrepareFunctionForOptimization(testFastPaths)');
testFastPaths(blockedFile);
eval('%OptimizeFunctionOnNextCall(testFastPaths)');
strictEqual(testFastPaths(blockedFile), 0);

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  strictEqual(getV8FastApiCallCount('fs.internalModuleStat'), 1);
}
