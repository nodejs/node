'use strict';

const { mustCall, mustNotCall } = require('../common');
const { open } = require('node:fs/promises');
const { setImmediate } = require('node:timers/promises');

function getHugeVector() {
  return new Array(1e5).fill(0).map((e, i) => i);
}

const registry = new FinalizationRegistry(() => {
  process.exitCode = 0;
  process.send(`success`);
});

async function run() {
  process.exitCode = 1;

  // Long-lived file handle. Ensure we keep a reference to it.
  const fh = await open(__filename)
  fh.on('close', mustNotCall())

  const stream = fh.createReadStream({ autoClose: false })
  // Keeping the file handle open shouldn't prevent the stream from being GCed.
  registry.register(stream, `[GC] Collected readable ${fh.fd}`);

  for await (const chunk of stream.iterator({ destroyOnReturn: false })) {
    break
  }

  // Keep a reference to the file handle
  return { fh };
}

async function forceGC() {
  gc();
  const hugeMatrix = [];
  for (let i = 0; i < 0x40; i++) {
    hugeMatrix.push(getHugeVector());
    gc();
    await setImmediate();
  }
}

run().then(async function (result) {
  await forceGC();
});
