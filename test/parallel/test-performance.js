'use strict';

const common = require('../common');
const assert = require('assert');
const { performance } = require('perf_hooks');

assert(performance);
assert(performance.nodeTiming);
assert.strictEqual(typeof performance.timeOrigin, 'number');

{
  const entries = performance.getEntries();
  assert(Array.isArray(entries));
  assert.strictEqual(entries.length, 1);
  assert.strictEqual(entries[0], performance.nodeTiming);
}

{
  const entries = performance.getEntriesByName('node');
  assert(Array.isArray(entries));
  assert.strictEqual(entries.length, 1);
  assert.strictEqual(entries[0], performance.nodeTiming);
}

{
  let n;
  let entries = performance.getEntries();
  for (n = 0; n < entries.length; n++) {
    const entry = entries[n];
    assert.notStrictEqual(entry.name, 'A');
    assert.notStrictEqual(entry.entryType, 'mark');
  }
  performance.mark('A');
  entries = performance.getEntries();
  const markA = entries[1];
  assert.strictEqual(markA.name, 'A');
  assert.strictEqual(markA.entryType, 'mark');
  performance.clearMarks('A');
  entries = performance.getEntries();
  for (n = 0; n < entries.length; n++) {
    const entry = entries[n];
    assert.notStrictEqual(entry.name, 'A');
    assert.notStrictEqual(entry.entryType, 'mark');
  }
}

{
  let entries = performance.getEntries();
  for (let n = 0; n < entries.length; n++) {
    const entry = entries[n];
    assert.notStrictEqual(entry.name, 'B');
    assert.notStrictEqual(entry.entryType, 'mark');
  }
  performance.mark('B');
  entries = performance.getEntries();
  const markB = entries[1];
  assert.strictEqual(markB.name, 'B');
  assert.strictEqual(markB.entryType, 'mark');
  performance.clearMarks();
  entries = performance.getEntries();
  assert.strictEqual(entries.length, 1);
}

{
  performance.mark('A');
  [undefined, null, 'foo', 'initialize', 1].forEach((i) => {
    assert.doesNotThrow(() => performance.measure('test', i, 'A'));
  });

  [undefined, null, 'foo', 1].forEach((i) => {
    assert.throws(() => performance.measure('test', 'A', i),
                  common.expectsError({
                    code: 'ERR_INVALID_PERFORMANCE_MARK',
                    type: Error,
                    message: `The "${i}" performance mark has not been set`
                  }));
  });

  performance.clearMeasures();
  performance.clearMarks();

  const entries = performance.getEntries();
  assert.strictEqual(entries.length, 1);
}

{
  performance.mark('A');
  setImmediate(() => {
    performance.mark('B');
    performance.measure('foo', 'A', 'B');
    const entry = performance.getEntriesByName('foo')[0];
    const markA = performance.getEntriesByName('A', 'mark')[0];
    performance.getEntriesByName('B', 'mark')[0];
    assert.strictEqual(entry.name, 'foo');
    assert.strictEqual(entry.entryType, 'measure');
    assert.strictEqual(entry.startTime, markA.startTime);
    // TODO(jasnell): This comparison is too imprecise on some systems
    //assert.strictEqual(entry.duration.toPrecision(3),
    //                   (markB.startTime - markA.startTime).toPrecision(3));
  });
}

assert.strictEqual(performance.nodeTiming.name, 'node');
assert.strictEqual(performance.nodeTiming.entryType, 'node');

[
  'startTime',
  'duration',
  'nodeStart',
  'v8Start',
  'bootstrapComplete',
  'environment',
  'loopStart',
  'loopExit',
  'thirdPartyMainStart',
  'thirdPartyMainEnd',
  'clusterSetupStart',
  'clusterSetupEnd',
  'moduleLoadStart',
  'moduleLoadEnd',
  'preloadModuleLoadStart',
  'preloadModuleLoadEnd'
].forEach((i) => {
  assert.strictEqual(typeof performance.nodeTiming[i], 'number');
});
