'use strict';

// This is a regression test for https://hackerone.com/reports/3511792

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { performance } = require('perf_hooks');
const fixtures = require('../common/fixtures');

const fixturePath = fixtures.path('array-hash-collision.js');

const t0 = performance.now();
const benignResult = spawnSync(process.execPath, [fixturePath, 'benign']);
const benignTime = performance.now() - t0;
assert.strictEqual(benignResult.status, 0);
console.log(`Benign test completed in ${benignTime.toFixed(2)}ms.`);

const t1 = performance.now();
const maliciousResult = spawnSync(process.execPath, [fixturePath, 'malicious'], {
  timeout: Math.ceil(benignTime * 10),
});
const maliciousTime = performance.now() - t1;
console.log(`Malicious test completed in ${maliciousTime.toFixed(2)}ms.`);

assert.strictEqual(maliciousResult.status, 0, `Hash flooding regression detected: ` +
  `Benign took ${benignTime}ms, malicious took more than ${maliciousTime}ms.`);
