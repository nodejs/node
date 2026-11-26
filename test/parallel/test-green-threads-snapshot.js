// Flags: --experimental-green-threads --no-warnings
'use strict';

// Green threads start from snapshots with their runtime already
// bootstrapped, in V8's default isolate group and in groups of their own.
// Each of them must still get state of its own.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { GreenThread, IsolateGroup } = require('node:green_threads');

const source = `
  const randoms = [];
  for (let i = 0; i < 4; i++) randoms.push(Math.random());
  const { port1, port2 } = new MessageChannel();
  port2.onmessage = ({ data }) => {
    postMessage({
      randoms,
      echoed: data,
      controller: typeof AbortController,
      // Relative to when the green thread started, not to when the
      // snapshot was made.
      timeStamp: new Event('x').timeStamp,
    });
    port2.close();
  };
  port1.postMessage('port');
`;

(async () => {
  const groups = ['default'];
  if (IsolateGroup.supported) groups.push('new', new IsolateGroup());
  for (const isolateGroup of groups) {
    const results = await Promise.all(Array.from({ length: 4 }, async () => {
      const greenThread = new GreenThread(source, { isolateGroup });
      const [result] = await once(greenThread, 'message');
      await greenThread.terminate();
      return result;
    }));
    const randoms = new Set(results.flatMap(({ randoms }) => randoms));
    assert.strictEqual(randoms.size, 16);
    for (const { echoed, controller, timeStamp } of results) {
      assert.strictEqual(echoed, 'port');
      assert.strictEqual(controller, 'function');
      assert(timeStamp >= 0 && timeStamp < 10_000, `${timeStamp}`);
    }
  }
})().then(common.mustCall());
