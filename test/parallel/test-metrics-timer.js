'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { timer, Timer, TimerFactory, Metric, MetricReport } = require('node:metrics');

const testTimer = timer('test', { base: 'test' });
assert.ok(testTimer instanceof TimerFactory);
assert.ok(testTimer instanceof Metric);

assert.strictEqual(testTimer.type, 'timer');
assert.strictEqual(testTimer.name, 'test');
assert.deepStrictEqual(testTimer.meta, { base: 'test' });
assert.strictEqual(testTimer.channelName, 'metrics:timer:test');

const a = testTimer.create({ timer: 'a' });
const b = testTimer.create({ timer: 'b' });

assert.ok(a instanceof Timer);
assert.ok(a instanceof Metric);

const messages = [
  [50, { base: 'test', timer: 'a', meta: 'extra' }],
  [100, { base: 'test', timer: 'b' }],
];

subscribe(testTimer.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'timer');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.ok(near(report.value, value));
  assert.deepStrictEqual(report.meta, meta);
}, 2));

// NOTE: If this test is flaky, tune the threshold to give more leeway to the timing
function near(actual, expected, threshold = 10) {
  return Math.abs(actual - expected) <= threshold;
}

setTimeout(common.mustCall(() => {
  a.stop({ meta: 'extra' });
  assert.ok(a.start > 0);
  assert.ok(a.end > 0);
  assert.ok(a.duration > 0);
}), 50);

setTimeout(common.mustCall(() => {
  b.stop();
}), 100);
