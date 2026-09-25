// Flags: --expose-gc --no-concurrent-array-buffer-sweeping
'use strict';

const common = require('../common');
const assert = require('assert');
const { setImmediate: setImmediatePromise } = require('timers/promises');

const MiB = 1024 * 1024;
const iterations = 64;
const maxRetained = 16 * MiB;

async function collectArrayBuffers() {
  for (let i = 0; i < 3; i++) {
    global.gc();
    await setImmediatePromise();
  }
}

async function assertNoBlobStreamRetention(name, fn) {
  const buffer = Buffer.alloc(MiB);

  await collectArrayBuffers();
  const before = process.memoryUsage().arrayBuffers;

  for (let i = 0; i < iterations; i++) {
    await fn(buffer);
  }

  await collectArrayBuffers();
  const retained = process.memoryUsage().arrayBuffers - before;

  assert(
    retained < maxRetained,
    `${name} retained ${retained} bytes in arrayBuffers`,
  );
}

(async () => {
  await assertNoBlobStreamRetention('unused Blob streams',
                                    common.mustCall(async (buffer) => {
                                      new Blob([buffer]).stream();
                                    }, iterations));

  await assertNoBlobStreamRetention('cancelled Blob streams',
                                    common.mustCall(async (buffer) => {
                                      await new Blob([buffer]).stream()
                                        .cancel();
                                    }, iterations));

  await assertNoBlobStreamRetention('drained Blob streams',
                                    common.mustCall(async (buffer) => {
                                      await new Response(
                                        new Blob([buffer]).stream(),
                                      ).arrayBuffer();
                                    }, iterations));
})().then(common.mustCall());
