// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { BlockList } = require('net');
const { internalBinding } = require('internal/test/binding');

const blockList = new BlockList();
blockList.addAddress('1.1.1.1');
blockList.addSubnet('10.0.0.0', 24);

function testFastCheck() {
  assert(blockList.check('1.1.1.1'));
  assert(!blockList.check('2.2.2.2'));
  assert(blockList.check('10.0.0.5'));
}

eval('%PrepareFunctionForOptimization(testFastCheck)');
testFastCheck();
eval('%OptimizeFunctionOnNextCall(testFastCheck)');
testFastCheck();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('blocklist.check'), 3);
}
