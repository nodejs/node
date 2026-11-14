// Flags: --experimental-green-threads --no-warnings
'use strict';

// Green threads in isolate groups of their own start from a snapshot with
// their runtime already bootstrapped. Each of them must still get state of
// its own.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { GreenThread, IsolateGroup } = require('node:green_threads');

if (!IsolateGroup.supported) {
  common.skip('isolate groups are not supported');
}

const source = `
  const randoms = [];
  for (let i = 0; i < 4; i++) randoms.push(Math.random());
  const { port1, port2 } = new MessageChannel();
  port2.onmessage = ({ data }) => {
    postMessage({ randoms, echoed: data, controller: typeof AbortController });
    port2.close();
  };
  port1.postMessage('port');
`;

(async () => {
  const shared = new IsolateGroup();
  for (const isolateGroup of ['new', shared]) {
    const results = await Promise.all(Array.from({ length: 4 }, async () => {
      const greenThread = new GreenThread(source, { isolateGroup });
      const [result] = await once(greenThread, 'message');
      await greenThread.terminate();
      return result;
    }));
    const randoms = new Set(results.flatMap(({ randoms }) => randoms));
    assert.strictEqual(randoms.size, 16);
    for (const { echoed, controller } of results) {
      assert.strictEqual(echoed, 'port');
      assert.strictEqual(controller, 'function');
    }
  }
})().then(common.mustCall());
