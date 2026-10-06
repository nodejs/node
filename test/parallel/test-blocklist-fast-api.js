// Flags: --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { BlockList } = require('net');
const { internalBinding } = require('internal/test/binding');

// The native check() method takes a SocketAddressBase object.
// String arguments go through checkString(), which also has a fast API.
const { kHandle: kBlockListHandle } = require('internal/blocklist');
const {
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');
const { AF_INET } = internalBinding('block_list');

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

function checkString(address) {
  return handle.checkString(address, AF_INET);
}

eval('%PrepareFunctionForOptimization(testFastCheck)');
testFastCheck();
eval('%OptimizeFunctionOnNextCall(testFastCheck)');
testFastCheck();

eval('%PrepareFunctionForOptimization(checkString)');
assert.strictEqual(checkString('1.1.1.1'), true);
eval('%OptimizeFunctionOnNextCall(checkString)');
assert.strictEqual(checkString('1.1.1.1'), true);
assert.strictEqual(checkString('2.2.2.2'), false);
assert.strictEqual(checkString('10.0.0.5'), true);

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('blocklist.check'), 3);
  assert.strictEqual(getV8FastApiCallCount('blocklist.checkString'), 3);
}
