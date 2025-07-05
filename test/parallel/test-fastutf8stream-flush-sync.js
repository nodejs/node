'use strict';

const { test, mock } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const { FastUtf8Stream } = require('node:fs');
const { tmpdir } = require('node:os');
const path = require('node:path');

let fileCounter = 0;

function getTempFile() {
  return path.join(tmpdir(), `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`flushSync - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 4096, sync });

    assert.ok(stream.write('hello world\n'));
    assert.ok(stream.write('something else\n'));

    stream.flushSync();

    // Let the file system settle down things
    await new Promise((resolve) => {
      setImmediate(() => {
        stream.end();
        const data = fs.readFileSync(dest, 'utf8');
        assert.strictEqual(data, 'hello world\nsomething else\n');

        stream.on('close', () => {
          // Cleanup
          try {
            fs.unlinkSync(dest);
          } catch (cleanupErr) {
            // Ignore cleanup errors
          }
          resolve();
        });
      });
    });
  });
}

test('retry in flushSync on EAGAIN', async (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false, minLength: 0 });

  await new Promise((resolve) => {
    stream.on('ready', () => {
      assert.ok(stream.write('hello world\n'));

      // Mock fs.writeSync to throw EAGAIN once
      let callCount = 0;
      const originalWriteSync = fs.writeSync;
      const mockWriteSync = mock.fn((fd, buf, enc) => {
        callCount++;
        if (callCount === 1) {
          const err = new Error('EAGAIN');
          err.code = 'EAGAIN';
          throw err;
        }
        // Call original function after first call
        return originalWriteSync.call(fs, fd, buf, enc);
      });

      mock.method(fs, 'writeSync', mockWriteSync);

      assert.ok(stream.write('something else\n'));

      // This should handle EAGAIN and retry
      stream.flushSync();
      stream.end();

      stream.on('finish', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data, 'hello world\nsomething else\n');

          // Cleanup
          try {
            fs.unlinkSync(dest);
          } catch (cleanupErr) {
            // Ignore cleanup errors
          }

          resolve();
        });
      });
    });
  });
});

test('throw error in flushSync on EAGAIN', async (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  let retryCallCount = 0;
  const stream = new FastUtf8Stream({
    fd,
    sync: false,
    minLength: 1000,
    retryEAGAIN: (err, writeBufferLen, remainingBufferLen) => {
      retryCallCount++;
      assert.strictEqual(err.code, 'EAGAIN');
      assert.strictEqual(writeBufferLen, 12);
      assert.strictEqual(remainingBufferLen, 0);
      return false; // Don't retry
    }
  });

  await new Promise((resolve) => {
    stream.on('ready', () => {
      // Mock fs.writeSync to throw EAGAIN
      const err = new Error('EAGAIN');
      err.code = 'EAGAIN';
      Error.captureStackTrace(err);

      const originalWriteSync = fs.writeSync;
      let mockCallCount = 0;
      const mockWriteSync = mock.fn((fd, buf, enc) => {
        mockCallCount++;
        if (mockCallCount === 1) {
          throw err;
        }
        // Call original function after first call
        return originalWriteSync.call(fs, fd, buf, enc);
      });

      mock.method(fs, 'writeSync', mockWriteSync);

      // Mock fs.fsyncSync
      const originalFsyncSync = fs.fsyncSync;
      const mockFsyncSync = mock.fn((...args) => {
        return originalFsyncSync.apply(fs, args);
      });

      mock.method(fs, 'fsyncSync', mockFsyncSync);

      assert.ok(stream.write('hello world\n'));

      // This should throw EAGAIN error
      assert.throws(() => {
        stream.flushSync();
      }, err);

      assert.ok(stream.write('something else\n'));
      stream.flushSync();

      stream.end();

      stream.on('finish', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data, 'hello world\nsomething else\n');

          // Verify retry callback was called
          assert.strictEqual(retryCallCount, 1);

          // Cleanup
          try {
            fs.unlinkSync(dest);
          } catch (cleanupErr) {
            // Ignore cleanup errors
          }

          resolve();
        });
      });
    });
  });
});
