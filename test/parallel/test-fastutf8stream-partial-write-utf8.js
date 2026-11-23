'use strict';

// Tests that no byte is lost or written twice when a partial write splits a
// multi-byte UTF-8 character.
// See: https://github.com/nodejs/node/issues/61744
// See: https://github.com/nodejs/node/issues/67679

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  openSync,
  readFileSync,
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

// Returns write() and writeSync() replacements whose first calls are short
// writes: call number i commits at most limits[i] bytes to the fd and reports
// that count, the way a full pipe or disk does. Later calls write everything.
function shortWrites(limits) {
  limits = [...limits];

  function commit(fd, data) {
    const bytes = Buffer.from(data);
    const limit = limits.length > 0 ? limits.shift() : bytes.length;
    const chunk = bytes.subarray(0, limit);
    let written = 0;
    while (written < chunk.length) {
      written += writeSync(fd, chunk, written);
    }
    return written;
  }

  return {
    write(fd, data, ...args) {
      process.nextTick(args.at(-1), null, commit(fd, data));
    },
    writeSync(fd, data) {
      return commit(fd, data);
    },
  };
}

const cases = [
  // "中" is e4 b8 ad. The first write stops after its first byte.
  { text: 'abc中def', limits: [4] },
  // The first write stops after the second byte of "中".
  { text: 'abc中def', limits: [5] },
  // "é" is c3 a9. The first write stops between its two bytes.
  { text: 'café!', limits: [4] },
  // "🌍" is f0 9f 8c 8d. The first write stops in the middle of it.
  { text: 'hello🌍world', limits: [7] },
  // The rest of "🌍" also needs more than one write.
  { text: 'hello🌍world', limits: [6, 1, 1] },
  // The whole string is a single character.
  { text: '中', limits: [1] },
  // Nothing is written at first.
  { text: '中文', limits: [0] },
  // The first write stops between two characters.
  { text: 'abc中def', limits: [3] },
  // Every write commits a single byte.
  { text: 'a中é🌍', limits: Array.from({ length: 40 }, () => 1) },
];

for (const sync of [false, true]) {
  for (const { text, limits } of cases) {
    // Short writes while the stream writes on its own.
    {
      const dest = getTempFile();
      const fd = openSync(dest, 'w');
      const stream = new Utf8Stream({ fd, sync, minLength: 0, fs: shortWrites(limits) });

      stream.on('ready', common.mustCall(() => {
        stream.write(text);
        stream.write(text);
        stream.end();
      }));

      stream.on('finish', common.mustCall());
      stream.on('close', common.mustCall(() => {
        assert.strictEqual(readFileSync(dest, 'utf8'), text + text);
      }));
    }

    // Short writes during flushSync().
    {
      const dest = getTempFile();
      const fd = openSync(dest, 'w');
      const stream = new Utf8Stream({ fd, sync, minLength: 4096, fs: shortWrites(limits) });

      stream.on('ready', common.mustCall(() => {
        stream.write(text);
        stream.write(text);
        stream.flushSync();
        assert.strictEqual(readFileSync(dest, 'utf8'), text + text);
        stream.end();
      }));

      stream.on('close', common.mustCall(() => {
        assert.strictEqual(readFileSync(dest, 'utf8'), text + text);
      }));
    }
  }
}
