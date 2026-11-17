// Flags: --expose-gc --expose-internals
'use strict';

// Another thread can write to a SharedArrayBuffer while buffer.transcode() or
// a StringDecoder is converting it. They must convert a copy of the memory: a
// conversion that measures its input in one pass and converts it in another
// overflows its output when the input changes in between.

const common = require('../common');
const assert = require('assert');
const { transcode } = require('buffer');
const { StringDecoder } = require('string_decoder');
const { Worker } = require('worker_threads');
const { internalBinding } = require('internal/test/binding');

// StringDecoder only decodes UTF-8 in more than one pass above 1 MiB.
const sab = new SharedArrayBuffer(2 * 1024 * 1024);

// Both byte pairs are valid UTF-8 and valid UTF-16LE, and each converts to a
// different number of code units than the other, in either direction. set()
// writes them to shared memory with atomic stores.
const worker = new Worker(`
  const { parentPort, workerData } = require('worker_threads');
  const shared = Buffer.from(workerData);
  const ascii = Buffer.alloc(shared.length, 'A\\0');
  const accented = Buffer.alloc(shared.length, 'é');
  parentPort.postMessage('ready');
  for (;;) {
    shared.set(ascii);
    shared.set(accented);
  }
`, { eval: true, workerData: sab });

worker.once('message', common.mustCall(() => {
  const shared = Buffer.from(sab);
  const decoder = new StringDecoder('utf8');

  for (let i = 0; i < 100; i++) {
    decoder.end(shared);
    if (common.hasIntl) {
      try {
        transcode(shared, 'utf8', 'ucs2');
      } catch (err) {
        // The copy can hold a character that was only half written.
        assert.strictEqual(err.code, 'U_INVALID_CHAR_FOUND');
      }
      // The high byte of every UTF-16LE code unit is 0x00 or 0xA9, so none of
      // them is a surrogate.
      transcode(shared, 'ucs2', 'utf8');
      // The binding also takes the SharedArrayBuffer itself.
      const result = internalBinding('icu').transcode(sab, 'ucs2', 'utf8');
      assert(Buffer.isBuffer(result));
    }
    // Each iteration leaves several MiB of results behind; collect them so
    // the test does not grow to hundreds of MiB before V8 does.
    globalThis.gc();
  }

  worker.terminate().then(common.mustCall());
}));
