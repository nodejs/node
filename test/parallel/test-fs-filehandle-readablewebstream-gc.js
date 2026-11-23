// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { readFileSync } = require('fs');
const { open } = require('fs/promises');
const { setImmediate: setImmediatePromise } = require('timers/promises');

const expected = readFileSync(__filename);

let closedOnGc = false;
process.on('warning', (warning) => {
  if (warning.code === 'DEP0137')
    closedOnGc = true;
});

// Return only the stream: the FileHandle must be kept alive by the stream
// itself, not by a reference in the caller.
async function openStream() {
  const file = await open(__filename);
  return file.readableWebStream();
}

(async () => {
  const readable = await openStream();

  // The original failure only appeared in long-lived processes with large
  // files. Give the GC repeated opportunities to collect the FileHandle
  // while the stream is still live.
  for (let i = 0; i < 10; i++) {
    global.gc();
    await setImmediatePromise();
  }

  const chunks = [];
  for await (const chunk of readable) {
    chunks.push(chunk);
    global.gc();
  }

  assert.strictEqual(Buffer.concat(chunks).compare(expected), 0,
                     'stream produced truncated or corrupted data');
  assert.strictEqual(closedOnGc, false,
                     'FileHandle was closed on garbage collection');
})().then(common.mustCall());
