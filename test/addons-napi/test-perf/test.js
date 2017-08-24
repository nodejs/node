'use strict';
const common = require('../../common');
const assert = require('assert');
const {
  performance,
  PerformanceObserver
} = require('perf_hooks');

// Testing api calls for arrays
const {
  TestMark,
  TestMeasure
} = require(`./build/${common.buildType}/test_perf`);

const markreg =
  /failed: Wrong type of argments\. Expects a string as first argument\./;

[1, {}, [], NaN, Infinity, () => {}].forEach((i) => {
  common.expectsError(() => TestMark(i),
                      {
                        type: Error,
                        message: markreg
                      });
});

const markA = new PerformanceObserver(common.mustCall((info) => {
  const marks = info.getEntries();
  assert.strictEqual(marks[0].name, 'A');
  markA.disconnect();
}));
markA.observe({ entryTypes: ['mark'] });

TestMark('A');

// busy loop for a bit
for (let n = 0; n < 1e6; n++) {}

const markB = new PerformanceObserver(common.mustCall((info) => {
  const marks = info.getEntries();
  assert.strictEqual(marks[0].name, 'B');
  markB.disconnect();
}));
markB.observe({ entryTypes: ['mark'] });

TestMark('B');

const measureA = new PerformanceObserver(common.mustCall((info) => {
  const measures = info.getEntries();
  assert.strictEqual(measures[0].name, 'A to B');
  measureA.disconnect();
  performance.clearMeasures();
  performance.clearMarks();
}));
measureA.observe({ entryTypes: ['measure'] });

TestMeasure('A to B', 'A', 'B');
