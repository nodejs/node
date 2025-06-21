'use strict';

const common = require('../common');
const cache = common;
console.log(cache);
const assert = require('assert');
const { BlockList } = require('net');
const data = [
  'Subnet: IPv4 192.168.1.0/24',
  'Address: IPv4 10.0.0.5',
  'Range: IPv4 192.168.2.1-192.168.2.10',
  'Range: IPv4 10.0.0.1-10.0.0.10',
  'Subnet: IPv6 2001:0db8:85a3:0000:0000:8a2e:0370:7334/64',
  'Address: IPv6 2001:0db8:85a3:0000:0000:8a2e:0370:7334',
  'Range: IPv6 2001:0db8:85a3:0000:0000:8a2e:0370:7334-2001:0db8:85a3:0000:0000:8a2e:0370:7335',
  'Subnet: IPv6 2001:db8:1234::/48',
  'Address: IPv6 2001:db8:1234::1',
  'Range: IPv6 2001:db8:1234::1-2001:db8:1234::10',
];
const test = new BlockList();
const test2 = new BlockList();
const test3 = new BlockList();
test.addAddress('127.0.0.1');
test.addAddress('192.168.0.1');
test2.fromJSON(JSON.parse(test.toJSON()));
test2.fromJSON(test.toJSON());
test3.fromJSON(data);
assert.strictEqual(test2.rules, test.rules);
assert.strictEqual(test3.rules, data);
