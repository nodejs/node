'use strict';

const common = require('../common');
const assert = require('assert');
const { performance } = require('perf_hooks');

if (!common.isMainThread)
  common.skip('bootstrapping workers works differently');

assert(performance);
assert(performance.nodeTiming);
assert.strictEqual(typeof performance.timeOrigin, 'number');
// Use a fairly large epsilon value, since we can only guarantee that the node
// process started up in 15 seconds.
assert(Math.abs(performance.timeOrigin - Date.now()) < 15000);

const inited = performance.now();
assert(inited < 15000);

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

let timeoutDelay = 111; // An extra of 111 ms for the first call.

function checkDelay(cb) {
  const defaultTimeout = 1;
  const timer = setInterval(checkDelay, defaultTimeout);
  const timeouts = 10;

  const now = getTime();
  let resolved = 0;

  function checkDelay() {
    resolved++;
    if (resolved === timeouts) {
      clearInterval(timer);
      timeoutDelay = getTime() - now;
      cb();
    }
  }
}

function getTime() {
  const ts = process.hrtime();
  return Math.ceil((ts[0] * 1e3) + (ts[1] / 1e6));
}

function checkNodeTiming(props) {
  console.log(props);

  for (const prop of Object.keys(props)) {
    if (props[prop].around !== undefined) {
      assert.strictEqual(typeof performance.nodeTiming[prop], 'number');
      const delta = performance.nodeTiming[prop] - props[prop].around;
      const delay = 1000 + timeoutDelay;
      assert(
        Math.abs(delta) < delay,
        `${prop}: ${Math.abs(delta)} >= ${delay}`
      );
    } else {
      assert.strictEqual(performance.nodeTiming[prop], props[prop],
                         `mismatch for performance property ${prop}: ` +
                         `${performance.nodeTiming[prop]} vs ${props[prop]}`);
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
  bootstrapComplete: { around: inited },
  environment: { around: 0 },
  loopStart: -1,
  loopExit: -1
});

checkDelay(() => {
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
      loopExit: -1
    });
  }, 1000);
});

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
    loopExit: { around: performance.now() }
  });
});
