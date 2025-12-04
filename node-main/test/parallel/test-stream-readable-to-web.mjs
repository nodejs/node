import { mustCall } from '../common/index.mjs';
import { Readable } from 'node:stream';
import { memoryUsage } from 'node:process';
import assert from 'node:assert';
import { setImmediate } from 'node:timers/promises';

// Based on: https://github.com/nodejs/node/issues/46347#issuecomment-1413886707
// edit: make it cross-platform as /dev/urandom is not available on Windows

const MAX_MEM = 256 * 1024 * 1024; // 256 MiB

function checkMemoryUsage() {
  assert(memoryUsage().arrayBuffers < MAX_MEM);
}

const MAX_BUFFERS = 1000;
let buffersCreated = 0;

const randomNodeStream = new Readable({
  read(size) {
    if (buffersCreated >= MAX_BUFFERS) {
      this.push(null);
      return;
    }

    this.push(Buffer.alloc(size));
    buffersCreated++;
  }
});

randomNodeStream.on('error', (err) => {
  assert.fail(err);
});

// Before doing anything, make sure memory usage is okay
checkMemoryUsage();

// Create stream and check memory usage remains okay

const randomWebStream = Readable.toWeb(randomNodeStream);

checkMemoryUsage();

let timeout;
try {
  // Wait two seconds before consuming the stream to see if memory usage increases
  timeout = setTimeout(mustCall(async () => {
    // Did the stream leak memory?
    checkMemoryUsage();
    // eslint-disable-next-line no-unused-vars
    for await (const _ of randomWebStream) {
      // Yield event loop to allow garbage collection
      await setImmediate();
      // consume the stream
      // check memory usage remains okay
      checkMemoryUsage();
    }
  }), 2000);
} catch (err) {
  if (timeout) {
    clearTimeout(timeout);
  }
  assert.fail(err);
}
