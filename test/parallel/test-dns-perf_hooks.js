'use strict';

const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const { PerformanceObserver } = require('perf_hooks');

const entries = [];
const obs = new PerformanceObserver(common.mustCallAtLeast((items) => {
  entries.push(...items.getEntries());
}));

obs.observe({ type: 'dns' });

dns.lookup('localhost', () => {});

process.on('exit', () => {
  assert.strictEqual(entries.length, 1);
  entries.forEach((entry) => {
    assert.strictEqual(!!entry.name, true);
    assert.strictEqual(entry.entryType, 'dns');
    assert.strictEqual(typeof entry.startTime, 'number');
    assert.strictEqual(typeof entry.duration, 'number');
    assert.strictEqual(typeof entry.detail, 'object');
  });
});
