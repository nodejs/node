'use strict';
// Regression test for https://github.com/nodejs/node/issues/67097
// worker.startCpuProfile() must return valid JSON when the profile contains a
// RegExp code frame whose name includes characters that need JSON escaping.
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const pattern = '^(\\d{6})';
const expectedFunctionName = `RegExp: ${pattern}`;

const worker = new Worker(`
  const re = new RegExp(${JSON.stringify(pattern)});
  setInterval(() => {
    let matches = 0;
    for (let i = 0; i < 30000; i++) {
      if (re.test(String(i))) matches++;
    }
    return matches;
  }, 0);
`, { eval: true });

worker.on('online', common.mustCall(async () => {
  try {
    let found = false;
    for (let attempt = 0; attempt < 50 && !found; attempt++) {
      const handle = await worker.startCpuProfile();
      await new Promise((resolve) => setTimeout(resolve, 100));
      const profile = JSON.parse(await handle.stop());
      found = profile.nodes.some(
        ({ callFrame }) => callFrame.functionName === expectedFunctionName);
    }
    assert.ok(found,
              `No "${expectedFunctionName}" frame found in the CPU profile`);
  } finally {
    await worker.terminate();
  }
}));
