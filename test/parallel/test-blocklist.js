'use strict';

require('../common');

const {
  BlockList,
  SocketAddress,
} = require('net');
const assert = require('assert');
const util = require('util');

{
  const blockList = new BlockList();

  [1, [], {}, null, 1n, undefined, null].forEach((i) => {
    assert.throws(() => blockList.addAddress(i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [1, [], {}, null, 1n, null].forEach((i) => {
    assert.throws(() => blockList.addAddress('1.1.1.1', i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  assert.throws(() => blockList.addAddress('1.1.1.1', 'foo'), {
    code: 'ERR_INVALID_ARG_VALUE'
  });

  [1, [], {}, null, 1n, undefined, null].forEach((i) => {
    assert.throws(() => blockList.addRange(i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
    assert.throws(() => blockList.addRange('1.1.1.1', i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [1, [], {}, null, 1n, null].forEach((i) => {
    assert.throws(() => blockList.addRange('1.1.1.1', '1.1.1.2', i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  assert.throws(() => blockList.addRange('1.1.1.1', '1.1.1.2', 'foo'), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
}

{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addAddress('8592:757c:efae:4e45:fb5d:d62a:0d00:8e17', 'ipv6');
  blockList.addAddress('::ffff:1.1.1.2', 'ipv6');

  assert(blockList.check('1.1.1.1'));
  assert(!blockList.check('1.1.1.1', 'ipv6'));
  assert(!blockList.check('8592:757c:efae:4e45:fb5d:d62a:0d00:8e17'));
  assert(blockList.check('8592:757c:efae:4e45:fb5d:d62a:0d00:8e17', 'ipv6'));

  assert(blockList.check('::ffff:1.1.1.1', 'ipv6'));
  assert(blockList.check('::ffff:1.1.1.1', 'IPV6'));

  assert(blockList.check('1.1.1.2'));

  assert(!blockList.check('1.2.3.4'));
  assert(!blockList.check('::1', 'ipv6'));
}

{
  const blockList = new BlockList();
  const sa1 = new SocketAddress({ address: '1.1.1.1' });
  const sa2 = new SocketAddress({
    address: '8592:757c:efae:4e45:fb5d:d62a:0d00:8e17',
    family: 'ipv6'
  });
  const sa3 = new SocketAddress({ address: '1.1.1.2' });

  blockList.addAddress(sa1);
  blockList.addAddress(sa2);
  blockList.addAddress('::ffff:1.1.1.2', 'ipv6');

  assert(blockList.check('1.1.1.1'));
  assert(blockList.check(sa1));
  assert(!blockList.check('1.1.1.1', 'ipv6'));
  assert(!blockList.check('8592:757c:efae:4e45:fb5d:d62a:0d00:8e17'));
  assert(blockList.check('8592:757c:efae:4e45:fb5d:d62a:0d00:8e17', 'ipv6'));
  assert(blockList.check(sa2));

  assert(blockList.check('::ffff:1.1.1.1', 'ipv6'));
  assert(blockList.check('::ffff:1.1.1.1', 'IPV6'));

  assert(blockList.check('1.1.1.2'));
  assert(blockList.check(sa3));

  assert(!blockList.check('1.2.3.4'));
  assert(!blockList.check('::1', 'ipv6'));
}

{
  const blockList = new BlockList();
  blockList.addRange('1.1.1.1', '1.1.1.10');
  blockList.addRange('::1', '::f', 'ipv6');

  assert(!blockList.check('1.1.1.0'));
  for (let n = 1; n <= 10; n++)
    assert(blockList.check(`1.1.1.${n}`));
  assert(!blockList.check('1.1.1.11'));

  assert(!blockList.check('::0', 'ipv6'));
  for (let n = 0x1; n <= 0xf; n++) {
    assert(blockList.check(`::${n.toString(16)}`, 'ipv6'),
           `::${n.toString(16)} check failed`);
  }
  assert(!blockList.check('::10', 'ipv6'));
}

{
  const blockList = new BlockList();
  const sa1 = new SocketAddress({ address: '1.1.1.1' });
  const sa2 = new SocketAddress({ address: '1.1.1.10' });
  const sa3 = new SocketAddress({ address: '::1', family: 'ipv6' });
  const sa4 = new SocketAddress({ address: '::f', family: 'ipv6' });

  blockList.addRange(sa1, sa2);
  blockList.addRange(sa3, sa4);

  assert(!blockList.check('1.1.1.0'));
  for (let n = 1; n <= 10; n++)
    assert(blockList.check(`1.1.1.${n}`));
  assert(!blockList.check('1.1.1.11'));

  assert(!blockList.check('::0', 'ipv6'));
  for (let n = 0x1; n <= 0xf; n++) {
    assert(blockList.check(`::${n.toString(16)}`, 'ipv6'),
           `::${n.toString(16)} check failed`);
  }
  assert(!blockList.check('::10', 'ipv6'));
}

{
  const blockList = new BlockList();
  blockList.addSubnet('1.1.1.0', 16);
  blockList.addSubnet('8592:757c:efae:4e45::', 64, 'ipv6');

  assert(blockList.check('1.1.0.1'));
  assert(blockList.check('1.1.1.1'));
  assert(!blockList.check('1.2.0.1'));
  assert(blockList.check('::ffff:1.1.0.1', 'ipv6'));

  assert(blockList.check('8592:757c:efae:4e45:f::', 'ipv6'));
  assert(blockList.check('8592:757c:efae:4e45::f', 'ipv6'));
  assert(!blockList.check('8592:757c:efae:4f45::f', 'ipv6'));
}

{
  const blockList = new BlockList();
  const sa1 = new SocketAddress({ address: '1.1.1.0' });
  const sa2 = new SocketAddress({ address: '1.1.1.1' });
  blockList.addSubnet(sa1, 16);
  blockList.addSubnet('8592:757c:efae:4e45::', 64, 'ipv6');

  assert(blockList.check('1.1.0.1'));
  assert(blockList.check(sa2));
  assert(!blockList.check('1.2.0.1'));
  assert(blockList.check('::ffff:1.1.0.1', 'ipv6'));

  assert(blockList.check('8592:757c:efae:4e45:f::', 'ipv6'));
  assert(blockList.check('8592:757c:efae:4e45::f', 'ipv6'));
  assert(!blockList.check('8592:757c:efae:4f45::f', 'ipv6'));
}

{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addRange('10.0.0.1', '10.0.0.10');
  blockList.addSubnet('8592:757c:efae:4e45::', 64, 'IpV6'); // Case insensitive

  const rulesCheck = [
    'Address: IPv4 1.1.1.1',
    'Subnet: IPv6 8592:757c:efae:4e45::/64',
    'Range: IPv4 10.0.0.1-10.0.0.10',
  ];
  assert.deepStrictEqual(blockList.rules.sort(), rulesCheck.sort());

  assert(blockList.check('1.1.1.1'));
  assert(blockList.check('10.0.0.5'));
  assert(blockList.check('::ffff:10.0.0.5', 'ipv6'));
  assert(blockList.check('8592:757c:efae:4e45::f', 'ipv6'));

  assert(!blockList.check('123.123.123.123'));
  assert(!blockList.check('8592:757c:efaf:4e45:fb5d:d62a:0d00:8e17', 'ipv6'));
  assert(!blockList.check('::ffff:123.123.123.123', 'ipv6'));
}

{
  // This test validates boundaries of non-aligned CIDR bit prefixes
  const blockList = new BlockList();
  blockList.addSubnet('10.0.0.0', 27);
  blockList.addSubnet('8592:757c:efaf::', 51, 'ipv6');

  for (let n = 0; n <= 31; n++)
    assert(blockList.check(`10.0.0.${n}`));
  assert(!blockList.check('10.0.0.32'));

  assert(blockList.check('8592:757c:efaf:0:0:0:0:0', 'ipv6'));
  assert(blockList.check('8592:757c:efaf:1fff:ffff:ffff:ffff:ffff', 'ipv6'));
  assert(!blockList.check('8592:757c:efaf:2fff:ffff:ffff:ffff:ffff', 'ipv6'));
}

{
  // Regression test for https://github.com/nodejs/node/issues/39074
  const blockList = new BlockList();

  blockList.addRange('10.0.0.2', '10.0.0.10');

  // IPv4 checks against IPv4 range.
  assert(blockList.check('10.0.0.2'));
  assert(blockList.check('10.0.0.10'));
  assert(!blockList.check('192.168.0.3'));
  assert(!blockList.check('2.2.2.2'));
  assert(!blockList.check('255.255.255.255'));

  // IPv6 checks against IPv4 range.
  assert(blockList.check('::ffff:0a00:0002', 'ipv6'));
  assert(blockList.check('::ffff:0a00:000a', 'ipv6'));
  assert(!blockList.check('::ffff:c0a8:0003', 'ipv6'));
  assert(!blockList.check('::ffff:0202:0202', 'ipv6'));
  assert(!blockList.check('::ffff:ffff:ffff', 'ipv6'));
}

{
  const blockList = new BlockList();
  assert.throws(() => blockList.addRange('1.1.1.2', '1.1.1.1'), /ERR_INVALID_ARG_VALUE/);
}

{
  const blockList = new BlockList();
  assert.throws(() => blockList.addSubnet(1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => blockList.addSubnet('1.1.1.1', ''),
                /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => blockList.addSubnet('1.1.1.1', NaN), /ERR_OUT_OF_RANGE/);
  assert.throws(() => blockList.addSubnet('', 1, 1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => blockList.addSubnet('', 1, ''), /ERR_INVALID_ARG_VALUE/);

  assert.throws(() => blockList.addSubnet('1.1.1.1', -1, 'ipv4'),
                /ERR_OUT_OF_RANGE/);
  assert.throws(() => blockList.addSubnet('1.1.1.1', 33, 'ipv4'),
                /ERR_OUT_OF_RANGE/);

  assert.throws(() => blockList.addSubnet('::', -1, 'ipv6'),
                /ERR_OUT_OF_RANGE/);
  assert.throws(() => blockList.addSubnet('::', 129, 'ipv6'),
                /ERR_OUT_OF_RANGE/);
}

{
  const blockList = new BlockList();
  assert.throws(() => blockList.check(1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => blockList.check('', 1), /ERR_INVALID_ARG_TYPE/);
}

{
  const blockList = new BlockList();
  const ret = util.inspect(blockList, { depth: -1 });
  assert.strictEqual(ret, '[BlockList]');
}

{
  const blockList = new BlockList();
  const ret = util.inspect(blockList, { depth: null });
  assert(ret.includes('rules: []'));
}

{
  // Test for https://github.com/nodejs/node/issues/43360
  const blocklist = new BlockList();
  blocklist.addSubnet('1.1.1.1', 32, 'ipv4');

  assert(blocklist.check('1.1.1.1'));
  assert(!blocklist.check('1.1.1.2'));
  assert(!blocklist.check('2.3.4.5'));
}

{
  assert(BlockList.isBlockList(new BlockList()));
  assert(!BlockList.isBlockList({}));
}

{
  // Test that adding the same address twice does not create duplicate rules.
  // Previously, the second add would orphan the first rule in the internal
  // list while overwriting its index entry, making it unreachable for removal
  // but still evaluated during checks.
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addAddress('1.1.1.1');

  // Should have exactly one rule, not two.
  assert.strictEqual(blockList.rules.length, 1);
  assert(blockList.check('1.1.1.1'));
}

{
  // Test clear() removes all rules.
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addRange('10.0.0.1', '10.0.0.10');
  blockList.addSubnet('192.168.0.0', 16);

  assert.strictEqual(blockList.rules.length, 3);
  assert(blockList.check('1.1.1.1'));
  assert(blockList.check('10.0.0.5'));
  assert(blockList.check('192.168.1.1'));

  blockList.clear();

  assert.strictEqual(blockList.rules.length, 0);
  assert(!blockList.check('1.1.1.1'));
  assert(!blockList.check('10.0.0.5'));
  assert(!blockList.check('192.168.1.1'));

  // Can add new rules after clearing.
  blockList.addAddress('2.2.2.2');
  assert.strictEqual(blockList.rules.length, 1);
  assert(blockList.check('2.2.2.2'));
  assert(!blockList.check('1.1.1.1'));
}

{
  // addAddresses() validation: non-array argument throws.
  const blockList0 = new BlockList();
  assert.throws(() => blockList0.addAddresses('not-an-array'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => blockList0.addAddresses(123), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

{
  // addAddresses() and addCIDRs() must throw the same errors for non-array
  // input regardless of how the checks are implemented internally.
  const blockList = new BlockList();
  for (const [value, received] of [
    ['x', "type string ('x')"],
    [123, 'type number (123)'],
    [{}, 'an instance of Object'],
    [null, 'null'],
    [undefined, 'undefined'],
    [1n, 'type bigint (1n)'],
    [true, 'type boolean (true)'],
  ]) {
    assert.throws(() => blockList.addAddresses(value), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "addresses" argument must be an instance of Array. ' +
               `Received ${received}`,
    });
    assert.throws(() => blockList.addCIDRs(value), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "cidrs" argument must be an instance of Array. ' +
               `Received ${received}`,
    });
  }
}

{
  // Test addAddresses() batch insert.
  const blockList = new BlockList();
  blockList.addAddresses(['1.1.1.1', '2.2.2.2', '3.3.3.3']);

  assert(blockList.check('1.1.1.1'));
  assert(blockList.check('2.2.2.2'));
  assert(blockList.check('3.3.3.3'));
  assert(!blockList.check('4.4.4.4'));
  assert.strictEqual(blockList.rules.length, 3);

  // Cross-family works with batch insert.
  assert(blockList.check('::ffff:1.1.1.1', 'ipv6'));

  // Batch with SocketAddress objects.
  const blockList2 = new BlockList();
  const sa1 = new SocketAddress({ address: '10.0.0.1' });
  const sa2 = new SocketAddress({ address: '10.0.0.2' });
  blockList2.addAddresses([sa1, sa2]);
  assert(blockList2.check('10.0.0.1'));
  assert(blockList2.check('10.0.0.2'));
  assert(!blockList2.check('10.0.0.3'));

  // IPv6 batch.
  const blockList3 = new BlockList();
  blockList3.addAddresses(['::1', '::2'], 'ipv6');
  assert(blockList3.check('::1', 'ipv6'));
  assert(blockList3.check('::2', 'ipv6'));
  assert(!blockList3.check('::3', 'ipv6'));
}

// Test exporting and importing the rule list to/from JSON
{
  const ruleList = [
    'Address: IPv4 10.0.0.5',
    'Address: IPv6 ::',
    'Subnet: IPv4 192.168.1.0/24',
    'Subnet: IPv6 8592:757c:efae:4e45::/64',
  ];

  const test2 = new BlockList();
  const test3 = new BlockList();
  const test4 = new BlockList();
  const test5 = new BlockList();

  const bl = new BlockList();
  bl.addAddress('10.0.0.5');
  bl.addAddress('::', 'ipv6');
  bl.addSubnet('192.168.1.0', 24);
  bl.addSubnet('8592:757c:efae:4e45::', 64, 'ipv6');

  // Test invalid inputs (input to fromJSON must be an array of
  // string rules or a serialized json string of an array of
  // string rules.
  [
    1, null, Symbol(), [1, 2, 3], '123', [Symbol()], new Map(),
  ].forEach((i) => {
    assert.throws(() => test2.fromJSON(i), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  // Invalid rules are ignored.
  test2.fromJSON(['1', '2', '3']);
  assert.deepStrictEqual(test2.rules, []);

  // Direct output from toJSON method works
  test2.fromJSON(bl.toJSON());
  assert.deepStrictEqual(test2.rules.sort(), ruleList);

  // JSON stringified output works
  test3.fromJSON(JSON.stringify(bl));
  assert.deepStrictEqual(test3.rules.sort(), ruleList);

  // A raw array works
  test4.fromJSON(ruleList);
  assert.deepStrictEqual(test4.rules.sort(), ruleList);

  // Individual rules work
  ruleList.forEach((item) => {
    test5.fromJSON([item]);
  });
  assert.deepStrictEqual(test5.rules.sort(), ruleList);

  // Each of the created blocklists should handle the checks identically.
  [
    ['10.0.0.5', 'ipv4', true],
    ['10.0.0.6', 'ipv4', false],
    ['::', 'ipv6', true],
    ['::1', 'ipv6', false],
    ['192.168.1.0', 'ipv4', true],
    ['193.168.1.0', 'ipv4', false],
    ['8592:757c:efae:4e45::', 'ipv6', true],
    ['1111:1111:1111:1111::', 'ipv6', false],
  ].forEach((i) => {
    assert.strictEqual(bl.check(i[0], i[1]), i[2]);
    assert.strictEqual(test2.check(i[0], i[1]), i[2]);
    assert.strictEqual(test3.check(i[0], i[1]), i[2]);
    assert.strictEqual(test4.check(i[0], i[1]), i[2]);
    assert.strictEqual(test5.check(i[0], i[1]), i[2]);
  });
}

// removeRange: basic removal
{
  const blockList = new BlockList();
  blockList.addRange('10.0.0.1', '10.0.0.100');
  blockList.addRange('192.168.1.1', '192.168.1.50');
  assert(blockList.check('10.0.0.50'));
  assert(blockList.check('192.168.1.25'));

  blockList.removeRange('10.0.0.1', '10.0.0.100');
  assert(!blockList.check('10.0.0.50'));
  assert(blockList.check('192.168.1.25'));
  assert.strictEqual(blockList.rules.length, 1);
}

// removeRange: non-existent range is a no-op
{
  const blockList = new BlockList();
  blockList.addRange('10.0.0.1', '10.0.0.10');
  assert.strictEqual(blockList.rules.length, 1);
  blockList.removeRange('99.99.99.1', '99.99.99.10');
  assert.strictEqual(blockList.rules.length, 1);
  assert(blockList.check('10.0.0.5'));
}

// removeRange: IPv6 range
{
  const blockList = new BlockList();
  blockList.addRange('2001:db8::1', '2001:db8::ff', 'ipv6');
  assert(blockList.check('2001:db8::50', 'ipv6'));

  blockList.removeRange('2001:db8::1', '2001:db8::ff', 'ipv6');
  assert(!blockList.check('2001:db8::50', 'ipv6'));
  assert.strictEqual(blockList.rules.length, 0);
}

// removeRange: with SocketAddress objects
{
  const blockList = new BlockList();
  const start = new SocketAddress({ address: '10.0.0.1' });
  const end = new SocketAddress({ address: '10.0.0.10' });
  blockList.addRange(start, end);
  assert(blockList.check('10.0.0.5'));

  blockList.removeRange(start, end);
  assert(!blockList.check('10.0.0.5'));
}

// removeSubnet: basic IPv4 removal
{
  const blockList = new BlockList();
  blockList.addSubnet('10.0.0.0', 8);
  blockList.addSubnet('192.168.0.0', 16);
  assert(blockList.check('10.1.2.3'));
  assert(blockList.check('192.168.5.5'));

  blockList.removeSubnet('10.0.0.0', 8);
  assert(!blockList.check('10.1.2.3'));
  assert(blockList.check('192.168.5.5'));
  assert.strictEqual(blockList.rules.length, 1);
}

// removeSubnet: IPv6
{
  const blockList = new BlockList();
  blockList.addSubnet('2001:db8::', 32, 'ipv6');
  assert(blockList.check('2001:db8::1', 'ipv6'));

  blockList.removeSubnet('2001:db8::', 32, 'ipv6');
  assert(!blockList.check('2001:db8::1', 'ipv6'));
  assert.strictEqual(blockList.rules.length, 0);
}

// removeSubnet: cross-family cleanup
{
  const blockList = new BlockList();
  blockList.addSubnet('10.0.0.0', 8);
  assert(blockList.check('::ffff:10.0.0.1', 'ipv6'));

  blockList.removeSubnet('10.0.0.0', 8);
  assert(!blockList.check('10.0.0.1'));
  assert(!blockList.check('::ffff:10.0.0.1', 'ipv6'));
}

// removeSubnet: non-existent subnet is a no-op
{
  const blockList = new BlockList();
  blockList.addSubnet('10.0.0.0', 8);
  blockList.removeSubnet('172.16.0.0', 12);
  assert(blockList.check('10.1.2.3'));
  assert.strictEqual(blockList.rules.length, 1);
}

// removeSubnet: with SocketAddress objects
{
  const blockList = new BlockList();
  const net = new SocketAddress({ address: '10.0.0.0' });
  blockList.addSubnet(net, 8);
  assert(blockList.check('10.1.2.3'));

  blockList.removeSubnet(net, 8);
  assert(!blockList.check('10.1.2.3'));
}

// removeRange/removeSubnet don't affect other rule types
{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addRange('10.0.0.1', '10.0.0.100');
  blockList.addSubnet('192.168.0.0', 16);

  blockList.removeRange('10.0.0.1', '10.0.0.100');
  blockList.removeSubnet('192.168.0.0', 16);

  // Address rule should still work
  assert(blockList.check('1.1.1.1'));
  assert(!blockList.check('10.0.0.50'));
  assert(!blockList.check('192.168.1.1'));
}

// addCIDR: IPv4
{
  const blockList = new BlockList();
  blockList.addCIDR('10.0.0.0/8');
  blockList.addCIDR('192.168.1.0/24');
  assert(blockList.check('10.1.2.3'));
  assert(blockList.check('192.168.1.50'));
  assert(!blockList.check('192.168.2.1'));
  assert(!blockList.check('11.0.0.1'));
}

// addCIDR: IPv6 auto-detected
{
  const blockList = new BlockList();
  blockList.addCIDR('2001:db8::/32');
  assert(blockList.check('2001:db8::1', 'ipv6'));
  assert(blockList.check('2001:db8:ffff::1', 'ipv6'));
  assert(!blockList.check('2001:db9::1', 'ipv6'));
}

// addCIDR: cross-family
{
  const blockList = new BlockList();
  blockList.addCIDR('10.0.0.0/8');
  assert(blockList.check('::ffff:10.0.0.1', 'ipv6'));
}

// addCIDR: validation errors
{
  const blockList = new BlockList();
  assert.throws(() => blockList.addCIDR('10.0.0.0'), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => blockList.addCIDR('10.0.0.0/abc'), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => blockList.addCIDR(123), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => blockList.addCIDR('10.0.0.0/'), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

// removeCIDR: basic
{
  const blockList = new BlockList();
  blockList.addCIDR('10.0.0.0/8');
  blockList.addCIDR('192.168.0.0/16');
  assert(blockList.check('10.1.2.3'));

  blockList.removeCIDR('10.0.0.0/8');
  assert(!blockList.check('10.1.2.3'));
  assert(blockList.check('192.168.1.1'));
  assert.strictEqual(blockList.rules.length, 1);
}

// removeCIDR: IPv6
{
  const blockList = new BlockList();
  blockList.addCIDR('2001:db8::/32');
  assert(blockList.check('2001:db8::1', 'ipv6'));

  blockList.removeCIDR('2001:db8::/32');
  assert(!blockList.check('2001:db8::1', 'ipv6'));
  assert.strictEqual(blockList.rules.length, 0);
}

// removeCIDR: non-existent is a no-op
{
  const blockList = new BlockList();
  blockList.addCIDR('10.0.0.0/8');
  blockList.removeCIDR('172.16.0.0/12');
  assert(blockList.check('10.1.2.3'));
  assert.strictEqual(blockList.rules.length, 1);
}

// addCIDR interoperates with removeSubnet, and vice versa
{
  const blockList = new BlockList();
  blockList.addCIDR('10.0.0.0/8');
  blockList.removeSubnet('10.0.0.0', 8);
  assert(!blockList.check('10.1.2.3'));

  blockList.addSubnet('192.168.0.0', 16);
  blockList.removeCIDR('192.168.0.0/16');
  assert(!blockList.check('192.168.1.1'));
}

// removeAddress: basic
{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addAddress('2.2.2.2');
  assert(blockList.check('1.1.1.1'));

  blockList.removeAddress('1.1.1.1');
  assert(!blockList.check('1.1.1.1'));
  assert(blockList.check('2.2.2.2'));
}

// removeAddress: cross-family cleanup
{
  const blockList = new BlockList();
  blockList.addAddress('3.3.3.3');
  assert(blockList.check('::ffff:3.3.3.3', 'ipv6'));

  blockList.removeAddress('3.3.3.3');
  assert(!blockList.check('3.3.3.3'));
  assert(!blockList.check('::ffff:3.3.3.3', 'ipv6'));
}

// removeAddress: IPv6
{
  const blockList = new BlockList();
  blockList.addAddress('::1', 'ipv6');
  assert(blockList.check('::1', 'ipv6'));

  blockList.removeAddress('::1', 'ipv6');
  assert(!blockList.check('::1', 'ipv6'));
}

// removeAddress: non-existent is a no-op
{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.removeAddress('9.9.9.9');
  assert(blockList.check('1.1.1.1'));
}

// removeAddress: with SocketAddress object
{
  const blockList = new BlockList();
  const addr = new SocketAddress({ address: '5.5.5.5' });
  blockList.addAddress(addr);
  assert(blockList.check('5.5.5.5'));

  blockList.removeAddress(addr);
  assert(!blockList.check('5.5.5.5'));
}

// addCIDRs: batch
{
  const blockList = new BlockList();
  blockList.addCIDRs(['10.0.0.0/8', '192.168.0.0/16', '2001:db8::/32']);
  assert(blockList.check('10.1.2.3'));
  assert(blockList.check('192.168.1.1'));
  assert(blockList.check('2001:db8::1', 'ipv6'));
  assert(!blockList.check('11.0.0.1'));
  assert.strictEqual(blockList.rules.length, 3);
}

// addCIDRs: validation
{
  const blockList = new BlockList();
  assert.throws(() => blockList.addCIDRs('not-an-array'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => blockList.addCIDRs([123]), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => blockList.addCIDRs(['10.0.0.0']), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

// addCIDRs: empty array is a no-op
{
  const blockList = new BlockList();
  blockList.addCIDRs([]);
  assert.strictEqual(blockList.size, 0);
}

// addCIDRs: invalid entry mid-array does not half-apply
{
  const blockList = new BlockList();
  assert.throws(() => blockList.addCIDRs(['10.0.0.0/8', 'bad', '1.1.1.0/24']), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  // Nothing should have been applied.
  assert.strictEqual(blockList.size, 0);
  assert(!blockList.check('10.0.0.1'));
}

// size: tracks all rule types
{
  const blockList = new BlockList();
  assert.strictEqual(blockList.size, 0);

  blockList.addAddress('1.1.1.1');
  assert.strictEqual(blockList.size, 1);

  blockList.addRange('10.0.0.1', '10.0.0.10');
  assert.strictEqual(blockList.size, 2);

  blockList.addSubnet('192.168.0.0', 16);
  assert.strictEqual(blockList.size, 3);

  // Matches rules.length
  assert.strictEqual(blockList.size, blockList.rules.length);

  blockList.removeAddress('1.1.1.1');
  assert.strictEqual(blockList.size, 2);

  blockList.removeRange('10.0.0.1', '10.0.0.10');
  assert.strictEqual(blockList.size, 1);

  blockList.removeSubnet('192.168.0.0', 16);
  assert.strictEqual(blockList.size, 0);

  // After clear
  blockList.addAddress('5.5.5.5');
  blockList.clear();
  assert.strictEqual(blockList.size, 0);
}

// size: duplicate addAddress does not double-count
{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  blockList.addAddress('1.1.1.1');
  assert.strictEqual(blockList.size, 1);
}

// PRIVATE_RANGES: is a frozen array of CIDR strings
{
  assert(Array.isArray(BlockList.PRIVATE_RANGES));
  assert(Object.isFrozen(BlockList.PRIVATE_RANGES));
  assert(BlockList.PRIVATE_RANGES.length > 0);
  for (const cidr of BlockList.PRIVATE_RANGES) {
    assert.strictEqual(typeof cidr, 'string');
    assert(cidr.includes('/'));
  }
}

// PRIVATE_RANGES: covers expected addresses
{
  const blockList = new BlockList();
  blockList.addCIDRs(BlockList.PRIVATE_RANGES);

  // IPv4 private (RFC 1918)
  assert(blockList.check('10.0.0.1'));
  assert(blockList.check('10.255.255.255'));
  assert(blockList.check('172.16.0.1'));
  assert(blockList.check('172.31.255.255'));
  assert(blockList.check('192.168.0.1'));
  assert(blockList.check('192.168.255.255'));

  // Loopback
  assert(blockList.check('127.0.0.1'));
  assert(blockList.check('127.255.255.255'));
  assert(blockList.check('::1', 'ipv6'));

  // Link-local
  assert(blockList.check('169.254.0.1'));
  assert(blockList.check('fe80::1', 'ipv6'));

  // ULA
  assert(blockList.check('fc00::1', 'ipv6'));
  assert(blockList.check('fd00::1', 'ipv6'));

  // Public addresses should not match
  assert(!blockList.check('8.8.8.8'));
  assert(!blockList.check('1.1.1.1'));
  assert(!blockList.check('203.0.113.1'));
  assert(!blockList.check('2001:db8::1', 'ipv6'));
}

// check() with invalid address string returns false (exercises checkString
// error path in C++ — SocketAddress::New fails, returns false).
{
  const blockList = new BlockList();
  blockList.addAddress('1.1.1.1');
  assert.strictEqual(blockList.check('not_a_valid_ip'), false);
  assert.strictEqual(blockList.check('', 'ipv4'), false);
  assert.strictEqual(blockList.check('999.999.999.999'), false);
  assert.strictEqual(blockList.check('not_valid_ipv6', 'ipv6'), false);
}

// check() family parameter is case-insensitive.
{
  const blockList = new BlockList();
  blockList.addAddress('10.0.0.1');
  blockList.addAddress('::1', 'ipv6');

  assert(blockList.check('10.0.0.1', 'ipv4'));
  assert(blockList.check('10.0.0.1', 'IPv4'));
  assert(blockList.check('10.0.0.1', 'IPV4'));
  assert(blockList.check('::1', 'ipv6'));
  assert(blockList.check('::1', 'IPv6'));
  assert(blockList.check('::1', 'IPV6'));
}

// SocketAddress constructor with invalid address throws ERR_INVALID_ADDRESS.
{
  assert.throws(() => new SocketAddress({ address: 'not_a_valid_ip' }), {
    code: 'ERR_INVALID_ADDRESS',
  });
  assert.throws(
    () => new SocketAddress({ address: 'not_valid', family: 'ipv6' }), {
      code: 'ERR_INVALID_ADDRESS',
    });
}

// check() with SocketAddress objects across family boundaries.
{
  const blockList = new BlockList();
  const ipv4 = new SocketAddress({ address: '10.0.0.1' });
  const mapped = new SocketAddress({
    address: '::ffff:10.0.0.1',
    family: 'ipv6',
  });

  blockList.addAddress(ipv4);

  // Check with SocketAddress objects (exercises the check() -> C++ fast path).
  assert(blockList.check(ipv4));
  assert(blockList.check(mapped));

  blockList.removeAddress(ipv4);
  assert(!blockList.check(ipv4));
  assert(!blockList.check(mapped));
}

// Subnet with IPv4-mapped IPv6 network.
{
  const blockList = new BlockList();
  blockList.addSubnet('::ffff:10.0.0.0', 120, 'ipv6');

  // IPv4-mapped IPv6 within the subnet should match.
  assert(blockList.check('::ffff:10.0.0.5', 'ipv6'));
  // The plain IPv4 form should also match (cross-family trie lookup).
  assert(blockList.check('10.0.0.5'));
  // Outside the subnet.
  assert(!blockList.check('10.0.1.0'));
}

// Range with IPv6 addresses.
{
  const blockList = new BlockList();
  blockList.addRange('::1', '::ff', 'ipv6');
  assert(blockList.check('::1', 'ipv6'));
  assert(blockList.check('::a0', 'ipv6'));
  assert(blockList.check('::ff', 'ipv6'));
  assert(!blockList.check('::100', 'ipv6'));
  assert(!blockList.check('::0', 'ipv6'));

  blockList.removeRange('::1', '::ff', 'ipv6');
  assert(!blockList.check('::a0', 'ipv6'));
}

// Removing a broader subnet must restore subsumed narrower subnets.
{
  const blockList = new BlockList();
  blockList.addSubnet('10.0.0.0', 8);   // /8 subsumes /16 in the trie
  blockList.addSubnet('10.1.0.0', 16);
  assert(blockList.check('10.1.2.3'));

  blockList.removeSubnet('10.0.0.0', 8);

  // /16 must still work after /8 is removed.
  assert(blockList.check('10.1.2.3'));
  // Address outside /16 but inside old /8 should no longer match.
  assert(!blockList.check('10.2.0.1'));
  assert.strictEqual(blockList.rules.length, 1);
}
