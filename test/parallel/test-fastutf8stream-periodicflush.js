'use strict';

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

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`periodicflush_off - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, sync, minLength: 5000 });

    try {
      assert.ok(stream.write('hello world\n'));

      // Wait 1.5 seconds - without periodicFlush, data should not be written
      await new Promise((resolve) => {
        setTimeout(() => {
          fs.readFile(dest, 'utf8', (err, data) => {
            assert.ifError(err);
            assert.strictEqual(data, '', 'file should be empty without periodicFlush');
            resolve();
          });
        }, 1500);
      });

      stream.destroy();
    } finally {
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`periodicflush property - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');

    try {
      // Test that periodicFlush property is set correctly
      const stream1 = new FastUtf8Stream({ fd, sync, minLength: 5000 });
      assert.strictEqual(stream1.periodicFlush, 0, 'default periodicFlush should be 0');
      stream1.destroy();

      const fd2 = fs.openSync(dest, 'w');
      const stream2 = new FastUtf8Stream({ fd: fd2, sync, minLength: 5000, periodicFlush: 1000 });
      assert.strictEqual(stream2.periodicFlush, 1000, 'periodicFlush should be set to 1000');
      stream2.destroy();
    } finally {
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`manual flush with minLength - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, sync, minLength: 5000 });

    try {
      assert.ok(stream.write('hello world\n'));

      // Manually flush to test that data can be written
      stream.flush((err) => {
        assert.ifError(err);
      });

      // Wait a bit for flush to complete
      await new Promise((resolve) => {
        setTimeout(() => {
          fs.readFile(dest, 'utf8', (err, data) => {
            assert.ifError(err);
            assert.strictEqual(data, 'hello world\n', 'file should contain data after manual flush');
            resolve();
          });
        }, 500);
      });

      stream.destroy();
    } finally {
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });
}
