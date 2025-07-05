'use strict';

const { test, afterEach } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const { FastUtf8Stream } = require('node:fs');
const { tmpdir } = require('node:os');
const path = require('node:path');

let fileCounter = 0;

function getTempFile() {
  return path.join(tmpdir(), `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

// Clean up all mocks after each test
afterEach(() => {
  // No mocks to clean up in this simplified version
});

test('fsync with sync', async (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  // Store original function
  const originalFsyncSync = fs.fsyncSync;
  let fsyncSyncCalls = 0;

  // Mock fs.fsyncSync to track calls
  const mockFsyncSync = (fd) => {
    fsyncSyncCalls++;
    return originalFsyncSync.call(fs, fd);
  };

  // Apply mock
  fs.fsyncSync = mockFsyncSync;

  try {
    const stream = new FastUtf8Stream({ fd, sync: true, fsync: true });

    assert.ok(stream.write('hello world\n'));
    assert.ok(stream.write('something else\n'));

    stream.end();

    const data = fs.readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world\nsomething else\n');

    // Verify fsyncSync was called
    assert.ok(fsyncSyncCalls > 0, 'fsyncSync should have been called');
  } finally {
    // Restore original function
    fs.fsyncSync = originalFsyncSync;

    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch (cleanupErr) {
      // Ignore cleanup errors
    }
  }
});

test('fsync with async', async (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  // Store original function
  const originalFsyncSync = fs.fsyncSync;
  let fsyncSyncCalls = 0;

  // Mock fs.fsyncSync to track calls
  const mockFsyncSync = (fd) => {
    fsyncSyncCalls++;
    return originalFsyncSync.call(fs, fd);
  };

  // Apply mock
  fs.fsyncSync = mockFsyncSync;

  try {
    const stream = new FastUtf8Stream({ fd, fsync: true });

    assert.ok(stream.write('hello world\n'));
    assert.ok(stream.write('something else\n'));

    stream.end();

    await new Promise((resolve, reject) => {
      stream.on('finish', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          try {
            assert.ifError(err);
            assert.strictEqual(data, 'hello world\nsomething else\n');
            resolve();
          } catch (assertErr) {
            reject(assertErr);
          }
        });
      });

      stream.on('close', () => {
        // Close emitted - test passed
      });
    });

    // Verify fsyncSync was called
    assert.ok(fsyncSyncCalls > 0, 'fsyncSync should have been called');
  } finally {
    // Restore original function
    fs.fsyncSync = originalFsyncSync;

    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch (cleanupErr) {
      // Ignore cleanup errors
    }
  }
});
