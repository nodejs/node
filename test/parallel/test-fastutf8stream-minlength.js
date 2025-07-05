'use strict';

require('../common');
const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const { FastUtf8Stream } = require('node:fs');
const { tmpdir } = require('node:os');
const path = require('node:path');

let fileCounter = 0;

function getTempFile() {
  return path.join(tmpdir(), `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

const MAX_WRITE = 16 * 1024;

test('drain deadlock', async (t) => {
  const dest = getTempFile();
  const stream = new FastUtf8Stream({ dest, sync: false, minLength: 9999 });

  try {
    assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
    assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
    assert.ok(!stream.write(Buffer.alloc(MAX_WRITE).fill('x').toString()));

    await new Promise((resolve) => {
      stream.on('drain', () => {
        resolve();
      });
    });

    stream.end();
  } finally {
    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch (err) {
      console.warn('Cleanup error:', err.message);
    }
  }
});

test('should throw if minLength >= maxWrite', (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  assert.throws(() => {
    new FastUtf8Stream({
      fd,
      minLength: MAX_WRITE
    });
  }, Error);

  // Cleanup
  try {
    fs.closeSync(fd);
    fs.unlinkSync(dest);
  } catch {
    // Ignore cleanup errors
  }
});
