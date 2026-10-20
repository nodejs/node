'use strict';

const common = require('../common.js');
const { BlockList, SocketAddress } = require('net');

const hasAddAddresses = typeof BlockList.prototype.addAddresses === 'function';

const operations = ['check', 'checkWithSocketAddress', 'addAddress'];
if (hasAddAddresses) {
  operations.push('addAddresses');
}

const bench = common.createBenchmark(main, {
  n: [1e6],
  ruleCount: [10, 100, 1000, 10000],
  ruleType: ['address', 'subnet', 'mixed'],
  checkResult: ['hit', 'miss'],
  operation: operations,
}, {
  combinationFilter({ operation, ruleCount, ruleType }) {
    // addAddress and addAddresses only need address rules, not subnets.
    if ((operation === 'addAddress' || operation === 'addAddresses') &&
        ruleType !== 'address') {
      return false;
    }
    return true;
  },
});

function generateIPv4(index) {
  return `${(index >>> 24) & 0xff}.${(index >>> 16) & 0xff}.` +
         `${(index >>> 8) & 0xff}.${index & 0xff}`;
}

function buildBlockList(ruleCount, ruleType) {
  const blockList = new BlockList();

  if (ruleType === 'address' || ruleType === 'mixed') {
    const addressCount = ruleType === 'mixed' ?
      Math.floor(ruleCount / 2) : ruleCount;
    const addresses = [];
    for (let i = 0; i < addressCount; i++) {
      // Start from 10.0.0.1 to avoid 0.0.0.0
      addresses.push(generateIPv4(0x0a000001 + i));
    }
    if (hasAddAddresses) {
      blockList.addAddresses(addresses);
    } else {
      for (const addr of addresses) {
        blockList.addAddress(addr);
      }
    }
  }

  if (ruleType === 'subnet' || ruleType === 'mixed') {
    const subnetCount = ruleType === 'mixed' ?
      Math.floor(ruleCount / 2) : ruleCount;
    for (let i = 0; i < subnetCount; i++) {
      // Use distinct /24 subnets: 172.i.j.0/24
      const second = (i >>> 8) & 0xff;
      const third = i & 0xff;
      blockList.addSubnet(`172.${second}.${third}.0`, 24);
    }
  }

  return blockList;
}

function main({ n, ruleCount, ruleType, checkResult, operation }) {
  if (operation === 'check') {
    benchCheck(n, ruleCount, ruleType, checkResult);
  } else if (operation === 'checkWithSocketAddress') {
    benchCheckWithSocketAddress(n, ruleCount, ruleType, checkResult);
  } else if (operation === 'addAddress') {
    benchAddAddress(n, ruleCount);
  } else if (operation === 'addAddresses') {
    benchAddAddresses(n, ruleCount);
  }
}

// Benchmark check() with string addresses (the common JS API path).
function benchCheck(n, ruleCount, ruleType, checkResult) {
  const blockList = buildBlockList(ruleCount, ruleType);

  // For 'hit', use an address that's in the list.
  // For 'miss', use an address that's not in the list.
  const address = checkResult === 'hit' ? '10.0.0.1' : '192.168.255.255';

  bench.start();
  for (let i = 0; i < n; i++) {
    blockList.check(address);
  }
  bench.end(n);
}

// Benchmark check() with pre-created SocketAddress objects
// (avoids measuring SocketAddress construction overhead).
function benchCheckWithSocketAddress(n, ruleCount, ruleType, checkResult) {
  const blockList = buildBlockList(ruleCount, ruleType);

  const address = checkResult === 'hit' ? '10.0.0.1' : '192.168.255.255';
  const sa = new SocketAddress({ address });

  bench.start();
  for (let i = 0; i < n; i++) {
    blockList.check(sa);
  }
  bench.end(n);
}

// Benchmark single addAddress() calls (one lock acquire per call).
function benchAddAddress(n, ruleCount) {
  // Scale n down for large rule counts to keep runtime reasonable.
  const iterations = Math.min(n, ruleCount * 100);

  const addresses = [];
  for (let i = 0; i < ruleCount; i++) {
    addresses.push(generateIPv4(0x0a000001 + i));
  }

  bench.start();
  for (let i = 0; i < iterations; i++) {
    const blockList = new BlockList();
    for (let j = 0; j < addresses.length; j++) {
      blockList.addAddress(addresses[j]);
    }
  }
  bench.end(iterations);
}

// Benchmark batch addAddresses() (one lock acquire per batch).
function benchAddAddresses(n, ruleCount) {
  const iterations = Math.min(n, ruleCount * 100);

  const addresses = [];
  for (let i = 0; i < ruleCount; i++) {
    addresses.push(generateIPv4(0x0a000001 + i));
  }

  bench.start();
  for (let i = 0; i < iterations; i++) {
    const blockList = new BlockList();
    blockList.addAddresses(addresses);
  }
  bench.end(iterations);
}
