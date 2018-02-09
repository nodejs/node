'use strict';

const common = require('../common');
const assert = require('assert');
const { performance } = require('perf_hooks');

assert(performance);
assert(performance.nodeTiming);
assert.strictEqual(typeof performance.timeOrigin, 'number');
// Use a fairly large epsilon value, since we can only guarantee that the node
// process started up in 20 seconds.
assert(Math.abs(performance.timeOrigin - Date.now()) < 20000);

const inited = performance.now();
assert(inited < 20000);

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
    performance.measure('test', i, 'A');
  });

  [undefined, null, 'foo', 1].forEach((i) => {
    common.expectsError(
      () => performance.measure('test', 'A', i),
      {
        code: 'ERR_INVALID_PERFORMANCE_MARK',
        type: Error,
        message: `The "${i}" performance mark has not been set`
      });
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

function checkNodeTiming(props) {
  for (const prop of Object.keys(props)) {
    if (props[prop].around !== undefined) {
      assert.strictEqual(typeof performance.nodeTiming[prop], 'number');
      const delta = performance.nodeTiming[prop] - props[prop].around;
      assert(Math.abs(delta) < 1000);
    } else {
      assert.strictEqual(performance.nodeTiming[prop], props[prop]);
    }
  }
}

checkNodeTiming({
  name: 'node',
  entryType: 'node',
  startTime: 0,
  duration: { around: performance.now() },
  nodeStart: { around: 0 },
  v8Start: { around: 0 },
  bootstrapComplete: -1,
  environment: { around: 0 },
  loopStart: -1,
  loopExit: -1,
  thirdPartyMainStart: -1,
  thirdPartyMainEnd: -1,
  clusterSetupStart: -1,
  clusterSetupEnd: -1,
  moduleLoadStart: { around: inited },
  moduleLoadEnd: { around: inited },
  preloadModuleLoadStart: { around: inited },
  preloadModuleLoadEnd: { around: inited },
});

setTimeout(() => {
  checkNodeTiming({
    name: 'node',
    entryType: 'node',
    startTime: 0,
    duration: { around: performance.now() },
    nodeStart: { around: 0 },
    v8Start: { around: 0 },
    bootstrapComplete: { around: inited },
    environment: { around: 0 },
    loopStart: { around: inited },
    loopExit: -1,
    thirdPartyMainStart: -1,
    thirdPartyMainEnd: -1,
    clusterSetupStart: -1,
    clusterSetupEnd: -1,
    moduleLoadStart: { around: inited },
    moduleLoadEnd: { around: inited },
    preloadModuleLoadStart: { around: inited },
    preloadModuleLoadEnd: { around: inited },
  });
}, 2000);

process.on('exit', () => {
  checkNodeTiming({
    name: 'node',
    entryType: 'node',
    startTime: 0,
    duration: { around: performance.now() },
    nodeStart: { around: 0 },
    v8Start: { around: 0 },
    bootstrapComplete: { around: inited },
    environment: { around: 0 },
    loopStart: { around: inited },
    loopExit: { around: performance.now() },
    thirdPartyMainStart: -1,
    thirdPartyMainEnd: -1,
    clusterSetupStart: -1,
    clusterSetupEnd: -1,
    moduleLoadStart: { around: inited },
    moduleLoadEnd: { around: inited },
    preloadModuleLoadStart: { around: inited },
    preloadModuleLoadEnd: { around: inited },
  });
});
