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
const { AF_INET, AF_INET6 } = internalBinding('block_list');

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

// An IPv6 string longer than the fast path's stack buffer (address part plus
// an overlong %zone) must give the same answer in the fast path as in the
// slow path. V8 only takes the fast path for flat one-byte strings.
{
  const mixed = 'ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255';
  const list = new BlockList();
  list.addAddress(mixed, 'ipv6');
  const handle6 = list[kBlockListHandle];
  const zoned = Buffer.from(`${mixed}%${'z'.repeat(200)}`).toString('latin1');
  function checkString6(address) {
    return handle6.checkString(address, AF_INET6);
  }
  eval('%PrepareFunctionForOptimization(checkString6)');
  const slow = checkString6(zoned);
  eval('%OptimizeFunctionOnNextCall(checkString6)');
  assert.strictEqual(checkString6(zoned), slow);
}
