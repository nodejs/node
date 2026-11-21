'use strict';
// Env: UV_THREADPOOL_SIZE=2
// fs.readFile() and fs.promises.readFile() stay abortable on a FIFO whose
// writer stays open, so the read never reaches EOF.
//
// The reader's open() holds a thread pool thread until a writer opens the
// FIFO, and the writer's open() needs a second one.
const common = require('../common');
if (common.isWindows)
  common.skip('no mkfifo on Windows');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { execFileSync } = require('child_process');
const fs = require('fs');
const { promisify } = require('util');

tmpdir.refresh();

async function testAbort(name, read) {
  const fifo = tmpdir.resolve(`${name}.fifo`);
  execFileSync('mkfifo', [fifo]);
  const ac = new AbortController();
  const settled = assert.rejects(read(fifo, { signal: ac.signal }), { name: 'AbortError' });
  const writer = await fs.promises.open(fifo, 'w');
  ac.abort();
  // A reader that honors the abort closes the FIFO after at most one more
  // read, and the next write fails with EPIPE. One that ignores it keeps
  // draining the FIFO until the writer closes.
  let closedByReader = false;
  const chunk = Buffer.alloc(1024);
  for (let i = 0; i < 1000; i++) {
    try {
      await writer.write(chunk);
    } catch (err) {
      assert.strictEqual(err.code, 'EPIPE');
      closedByReader = true;
      break;
    }
  }
  await writer.close();
  assert.ok(closedByReader, `${name} kept reading after the abort`);
  await settled;
}

(async () => {
  await testAbort('callback', promisify(fs.readFile));
  await testAbort('promises', fs.promises.readFile);
})().then(common.mustCall());
