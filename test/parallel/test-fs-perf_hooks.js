'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { PerformanceObserver } = require('perf_hooks');

const obs = new PerformanceObserver(common.mustCallAtLeast((items) => {
  items.getEntries().forEach((entry) => {
    assert.strictEqual(entry.entryType, 'fs');
    assert.strictEqual(typeof entry.startTime, 'number');
    assert.strictEqual(typeof entry.duration, 'number');
  });
}));

obs.observe({ type: 'fs' });
fs.readFileSync(__filename);
