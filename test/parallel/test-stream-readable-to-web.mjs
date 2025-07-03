import { hasCrypto, skip } from '../common/index.mjs';
if (!hasCrypto) { skip('missing crypto'); }

import { Readable } from 'stream';
import { memoryUsage } from 'process';
import { randomBytes } from 'crypto';
import assert from 'assert';
import { setImmediate } from 'timers/promises';

// Based on: https://github.com/nodejs/node/issues/46347#issuecomment-1413886707
// edit: make it cross-platform as /dev/urandom is not available on Windows

const MAX_MEM = 256 * 1024 * 1024; // 256 MiB

function checkMemoryUsage() {
  assert(memoryUsage().arrayBuffers < MAX_MEM);
}

let end = false;

const timeout = setTimeout(() => {
  end = true;
}, 5000);

const randomNodeStream = new Readable({
  read(size) {
    if (end) {
      this.push(null);
      return;
    }

    randomBytes(size, (err, buf) => {
      if (err) {
        this.destroy(err);
        return;
      }

      this.push(buf);
    });
  }
});

randomNodeStream.on('error', (err) => {
  clearTimeout(timeout);
  assert.fail(err);
});

// Before doing anything, make sure memory usage is okay
checkMemoryUsage();

// Create stream and check memory usage remains okay

const randomWebStream = Readable.toWeb(randomNodeStream);

checkMemoryUsage();

try {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of randomWebStream) {
    // Yield event loop to allow garbage collection
    await setImmediate();
    // consume the stream
    // check memory usage remains okay
    checkMemoryUsage();
  }
} catch (err) {
  clearTimeout(timeout);
  assert.fail(err);
}
