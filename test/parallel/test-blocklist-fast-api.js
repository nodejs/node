// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { BlockList } = require('net');
const { internalBinding } = require('internal/test/binding');

// The fast API is on the native check() method which takes a
// SocketAddressBase object. The JS BlockList.prototype.check() routes
// string arguments to checkString() which has no fast API, so we need
// to use SocketAddress objects to exercise the fast API path.
const { kHandle: kBlockListHandle } = require('internal/blocklist');
const {
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');

const blockList = new BlockList();
blockList.addAddress('1.1.1.1');
blockList.addSubnet('10.0.0.0', 24);

const handle = blockList[kBlockListHandle];
const addr1 = new SocketAddress({ address: '1.1.1.1' })[kSocketAddressHandle];
const addr2 = new SocketAddress({ address: '2.2.2.2' })[kSocketAddressHandle];
const addr3 = new SocketAddress({ address: '10.0.0.5' })[kSocketAddressHandle];

function testFastCheck() {
  assert.strictEqual(handle.check(addr1), true);
  assert.strictEqual(handle.check(addr2), false);
  assert.strictEqual(handle.check(addr3), true);
}

eval('%PrepareFunctionForOptimization(testFastCheck)');
testFastCheck();
eval('%OptimizeFunctionOnNextCall(testFastCheck)');
testFastCheck();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('blocklist.check'), 3);
}
