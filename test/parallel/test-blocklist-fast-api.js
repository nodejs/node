// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { BlockList } = require('net');
const { internalBinding } = require('internal/test/binding');

const { SocketAddress } = require('net');

const blockList = new BlockList();
blockList.addAddress('1.1.1.1');
blockList.addSubnet('10.0.0.0', 24);

// FastCheck requires SocketAddress objects, not strings.
// Strings go through the checkString path instead.
const addr1 = new SocketAddress({ address: '1.1.1.1' });
const addr2 = new SocketAddress({ address: '2.2.2.2' });
const addr3 = new SocketAddress({ address: '10.0.0.5' });

function testFastCheck() {
  assert(blockList.check(addr1));
  assert(!blockList.check(addr2));
  assert(blockList.check(addr3));
}

eval('%PrepareFunctionForOptimization(testFastCheck)');
testFastCheck();
eval('%OptimizeFunctionOnNextCall(testFastCheck)');
testFastCheck();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('blocklist.check'), 3);
}
