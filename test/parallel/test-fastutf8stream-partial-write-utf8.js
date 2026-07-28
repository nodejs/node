'use strict';

// Tests for UTF-8 character preservation when partial writes split multi-byte characters.
// See: https://github.com/nodejs/node/issues/61744

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  openSync,
  write,
  writeSync,
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { join } = require('node:path');
const { isMainThread } = require('node:worker_threads');

tmpdir.refresh();
if (isMainThread) {
  process.umask(0o000);
}

let fileCounter = 0;

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-partial-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

runTests(false);
runTests(true);

function runTests(sync) {
  // Test 1: Partial write splitting a 3-byte UTF-8 character (CJK)
  // "abc中def" where "中" is 3 bytes (E4 B8 AD)
  // Simulate partial write of 4 bytes: "abc" (3 bytes) + first byte of "中"
  // The remaining buffer should be "中def" (not "def")
  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    let firstWrite = true;
    const writtenChunks = [];
    const fsOverride = {};

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        if (firstWrite) {
          firstWrite = false;
          // Simulate partial write: only 4 bytes written out of 9
          // This splits the 3-byte "中" character
          return 4;
        }
        return writeSync(...args);
      }, 2);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        const callback = args[args.length - 1];
        if (firstWrite) {
          firstWrite = false;
          // Simulate partial write: only 4 bytes written out of 9
          process.nextTick(callback, null, 4);
          return;
        }
        return write(...args);
      }, 2);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 0,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      stream.write('abc中def');
      stream.end();

      stream.on('finish', common.mustCall(() => {
        // Verify the second chunk contains the preserved CJK character
        assert.strictEqual(writtenChunks.length, 2);
        assert.strictEqual(writtenChunks[0], 'abc中def'); // First attempt
        assert.strictEqual(writtenChunks[1], '中def'); // Retry with preserved char
      }));
    }));
  }

  // Test 2: Partial write splitting a 4-byte UTF-8 character (emoji)
  // "hello🌍world" where "🌍" is 4 bytes (F0 9F 8C 8D)
  // Simulate partial write of 7 bytes: "hello" (5 bytes) + first 2 bytes of "🌍"
  // The remaining buffer should be "🌍world" (not a lone surrogate + "world")
  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    let firstWrite = true;
    const writtenChunks = [];
    const fsOverride = {};

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        if (firstWrite) {
          firstWrite = false;
          // Simulate partial write: only 7 bytes written
          return 7;
        }
        return writeSync(...args);
      }, 2);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        const callback = args[args.length - 1];
        if (firstWrite) {
          firstWrite = false;
          process.nextTick(callback, null, 7);
          return;
        }
        return write(...args);
      }, 2);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 0,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      stream.write('hello🌍world');
      stream.end();

      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(writtenChunks.length, 2);
        assert.strictEqual(writtenChunks[0], 'hello🌍world'); // First attempt
        assert.strictEqual(writtenChunks[1], '🌍world'); // Retry with preserved emoji

        // Verify no lone surrogates in the retry chunk
        const retryChunk = writtenChunks[1];
        for (let i = 0; i < retryChunk.length; i++) {
          const code = retryChunk.charCodeAt(i);
          if (code >= 0xD800 && code <= 0xDBFF) {
            // High surrogate - next must be low surrogate
            const next = retryChunk.charCodeAt(i + 1);
            assert.ok(next >= 0xDC00 && next <= 0xDFFF,
                      `Found lone high surrogate at position ${i}`);
            i++; // Skip the low surrogate we just verified
          } else if (code >= 0xDC00 && code <= 0xDFFF) {
            // Low surrogate without preceding high surrogate
            assert.fail(`Found lone low surrogate at position ${i}: 0x${code.toString(16)}`);
          }
        }
      }));
    }));
  }

  // Test 3: Partial write at exactly 0 bytes (edge case)
  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    let firstWrite = true;
    const writtenChunks = [];
    const fsOverride = {};

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        if (firstWrite) {
          firstWrite = false;
          return 0; // No bytes written
        }
        return writeSync(...args);
      }, 2);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        const callback = args[args.length - 1];
        if (firstWrite) {
          firstWrite = false;
          process.nextTick(callback, null, 0);
          return;
        }
        return write(...args);
      }, 2);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 0,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      stream.write('中文');
      stream.end();

      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(writtenChunks.length, 2);
        assert.strictEqual(writtenChunks[0], '中文');
        assert.strictEqual(writtenChunks[1], '中文'); // Entire string retried
      }));
    }));
  }

  // Test 4: Partial write splitting between characters (not mid-character)
  // This should work the same as before - no character preservation needed
  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    let firstWrite = true;
    const writtenChunks = [];
    const fsOverride = {};

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        if (firstWrite) {
          firstWrite = false;
          // Write exactly 3 bytes ("abc"), which is a clean character boundary
          return 3;
        }
        return writeSync(...args);
      }, 2);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        const callback = args[args.length - 1];
        if (firstWrite) {
          firstWrite = false;
          process.nextTick(callback, null, 3);
          return;
        }
        return write(...args);
      }, 2);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 0,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      stream.write('abc中def');
      stream.end();

      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(writtenChunks.length, 2);
        assert.strictEqual(writtenChunks[0], 'abc中def');
        assert.strictEqual(writtenChunks[1], '中def'); // Remaining after 3 bytes
      }));
    }));
  }

  // Test 5: Single multi-byte character with partial write of 1 byte
  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    let firstWrite = true;
    const writtenChunks = [];
    const fsOverride = {};

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        if (firstWrite) {
          firstWrite = false;
          // Write only 1 byte of a 3-byte character
          return 1;
        }
        return writeSync(...args);
      }, 2);
    } else {
      fsOverride.write = common.mustCall((...args) => {
        const data = args[1];
        writtenChunks.push(typeof data === 'string' ? data : data.toString());
        const callback = args[args.length - 1];
        if (firstWrite) {
          firstWrite = false;
          process.nextTick(callback, null, 1);
          return;
        }
        return write(...args);
      }, 2);
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 0,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      stream.write('中');
      stream.end();

      stream.on('finish', common.mustCall(() => {
        assert.strictEqual(writtenChunks.length, 2);
        assert.strictEqual(writtenChunks[0], '中');
        assert.strictEqual(writtenChunks[1], '中'); // Full character retried
      }));
    }));
  }
}
