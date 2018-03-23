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
  // Should work without throwing any errors
  performance.mark('A');
  performance.clearMarks('A');

  performance.mark('B');
  performance.clearMarks();
}

{
  performance.mark('A');
  [undefined, null, 'foo', 'initialize', 1].forEach((i) => {
    performance.measure('test', i, 'A'); // Should not throw.
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

  performance.clearMarks();
}

{
  performance.mark('A');
  setImmediate(() => {
    performance.mark('B');
    performance.measure('foo', 'A', 'B');
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
