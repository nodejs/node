'use strict';

const { test, mock, afterEach } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { FastUtf8Stream } = require('node:fs');
const { tmpdir } = require('node:os');

let fileCounter = 0;

function getTempFile() {
  return path.join(tmpdir(), `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

// Clean up all mocks after each test
afterEach(() => {
  mock.restoreAll();
});

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`only call fsyncSync and not fsync when fsync: true - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({
      fd,
      sync,
      fsync: true,
      minLength: 4096
    });

    // Store original functions
    const originalFsync = fs.fsync;
    const originalFsyncSync = fs.fsyncSync;
    const originalWrite = fs.write;
    const originalWriteSync = fs.writeSync;

    let fsyncCalls = 0;
    let fsyncSyncCalls = 0;
    let writeCalls = 0;

    // Mock fs.fsync to track calls (should not be called)
    const mockFsync = (fd, cb) => {
      fsyncCalls++;
      cb(new Error('fs.fsync should not be called'));
    };

    // Mock fs.fsyncSync to track calls (should be called)
    const mockFsyncSync = (fd) => {
      fsyncSyncCalls++;
      return originalFsyncSync.call(fs, fd);
    };

    // Mock write functions to track calls
    const mockWrite = (...args) => {
      writeCalls++;
      return originalWrite.call(fs, ...args);
    };

    const mockWriteSync = (...args) => {
      writeCalls++;
      return originalWriteSync.call(fs, ...args);
    };

    // Apply mocks
    fs.fsync = mockFsync;
    fs.fsyncSync = mockFsyncSync;
    if (sync) {
      fs.writeSync = mockWriteSync;
    } else {
      fs.write = mockWrite;
    }

    try {
      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          assert.ok(stream.write('hello world\n'));

          stream.flush((err) => {
            try {
              assert.ifError(err);
              // Verify fsyncSync was called
              assert.strictEqual(fsyncSyncCalls, 1, 'fsyncSync should be called once');
              // Verify fsync was not called
              assert.strictEqual(fsyncCalls, 0, 'fsync should not be called');
              // Verify write was called
              assert.strictEqual(writeCalls, 1, 'write should be called once');

              stream.end();
              resolve();
            } catch (assertErr) {
              reject(assertErr);
            }
          });
        });
      });
    } finally {
      // Restore original functions
      fs.fsync = originalFsync;
      fs.fsyncSync = originalFsyncSync;
      fs.write = originalWrite;
      fs.writeSync = originalWriteSync;

      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`call flush cb with error when fsync failed - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({
      fd,
      sync,
      minLength: 4096
    });

    // Store original functions
    const originalFsync = fs.fsync;
    const originalWrite = fs.write;
    const originalWriteSync = fs.writeSync;

    const testError = new Error('fsync failed');
    testError.code = 'ETEST';

    // Mock fs.fsync to fail
    const mockFsync = (fd, cb) => {
      process.nextTick(() => cb(testError));
    };

    // Mock write functions to succeed
    const mockWrite = (...args) => {
      return originalWrite.call(fs, ...args);
    };

    const mockWriteSync = (...args) => {
      return originalWriteSync.call(fs, ...args);
    };

    // Apply mocks
    fs.fsync = mockFsync;
    if (sync) {
      fs.writeSync = mockWriteSync;
    } else {
      fs.write = mockWrite;
    }

    try {
      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          assert.ok(stream.write('hello world\n'));

          stream.flush((err) => {
            try {
              assert.ok(err, 'flush should return an error');
              assert.strictEqual(err.code, 'ETEST', 'error should have correct code');

              stream.end();
              resolve();
            } catch (assertErr) {
              reject(assertErr);
            }
          });
        });
      });
    } finally {
      // Restore original functions
      fs.fsync = originalFsync;
      fs.write = originalWrite;
      fs.writeSync = originalWriteSync;

      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

}
