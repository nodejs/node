'use strict';

require('../common');
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
  // Restore original functions if they were mocked
  if (fs.write.isMocked) {
    fs.write = fs.write.original;
  }
  if (fs.writeSync.isMocked) {
    fs.writeSync = fs.writeSync.original;
  }
});

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`retry on EAGAIN - sync: ${sync}`, async () => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');

    // Store original function
    const originalWrite = sync ? fs.writeSync : fs.write;
    let callCount = 0;

    try {
      if (sync) {
        fs.writeSync = function(fd, buf, enc) {
          callCount++;
          if (callCount === 1) {
            const err = new Error('EAGAIN');
            err.code = 'EAGAIN';
            throw err;
          }
          return originalWrite.call(fs, fd, buf, enc);
        };
        fs.writeSync.isMocked = true;
        fs.writeSync.original = originalWrite;
      } else {
        fs.write = function(fd, buf, ...args) {
          callCount++;
          if (callCount === 1) {
            const err = new Error('EAGAIN');
            err.code = 'EAGAIN';
            const callback = args[args.length - 1];
            process.nextTick(callback, err);
            return;
          }
          return originalWrite.call(fs, fd, buf, ...args);
        };
        fs.write.isMocked = true;
        fs.write.original = originalWrite;
      }

      const stream = new FastUtf8Stream({ fd, sync, minLength: 0 });

      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          assert.ok(stream.write('hello world\n'));
          assert.ok(stream.write('something else\n'));

          stream.end();

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
        });
      });
    } finally {
      // Restore original function
      if (sync) {
        fs.writeSync = originalWrite;
      } else {
        fs.write = originalWrite;
      }

      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (err) {
        console.warn('Cleanup error:', err.message);
      }
    }
  });
}

test('emit error on async EAGAIN', async () => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  // Store original function
  const originalWrite = fs.write;
  let callCount = 0;

  try {
    fs.write = function(fd, buf, ...args) {
      callCount++;
      if (callCount === 1) {
        const err = new Error('EAGAIN');
        err.code = 'EAGAIN';
        const callback = args[args.length - 1];
        process.nextTick(callback, err);
        return;
      }
      return originalWrite.call(fs, fd, buf, ...args);
    };

    const stream = new FastUtf8Stream({
      fd,
      sync: false,
      minLength: 12,
      retryEAGAIN: (err, writeBufferLen, remainingBufferLen) => {
        assert.strictEqual(err.code, 'EAGAIN');
        assert.strictEqual(writeBufferLen, 12);
        assert.strictEqual(remainingBufferLen, 0);
        return false; // Don't retry
      }
    });

    await new Promise((resolve, reject) => {
      stream.on('ready', () => {
        stream.once('error', (err) => {
          assert.strictEqual(err.code, 'EAGAIN');
          assert.ok(stream.write('something else\n'));
        });

        assert.ok(stream.write('hello world\n'));

        stream.end();

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
      });
    });
  } finally {
    // Restore original function
    fs.write = originalWrite;

    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch {
      // Ignore cleanup errors
    }
  }
});

test('retry on EBUSY', async () => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  // Store original function
  const originalWrite = fs.write;
  let callCount = 0;

  try {
    fs.write = function(fd, buf, ...args) {
      callCount++;
      if (callCount === 1) {
        const err = new Error('EBUSY');
        err.code = 'EBUSY';
        const callback = args[args.length - 1];
        process.nextTick(callback, err);
        return;
      }
      return originalWrite.call(fs, fd, buf, ...args);
    };

    const stream = new FastUtf8Stream({ fd, sync: false, minLength: 0 });

    await new Promise((resolve, reject) => {
      stream.on('ready', () => {
        assert.ok(stream.write('hello world\n'));
        assert.ok(stream.write('something else\n'));

        stream.end();

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
      });
    });
  } finally {
    // Restore original function
    fs.write = originalWrite;

    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch {
      // Ignore cleanup errors
    }
  }
});

test('emit error on async EBUSY', async () => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  // Store original function
  const originalWrite = fs.write;
  let callCount = 0;

  try {
    fs.write = function(fd, buf, ...args) {
      callCount++;
      if (callCount === 1) {
        const err = new Error('EBUSY');
        err.code = 'EBUSY';
        const callback = args[args.length - 1];
        process.nextTick(callback, err);
        return;
      }
      return originalWrite.call(fs, fd, buf, ...args);
    };

    const stream = new FastUtf8Stream({
      fd,
      sync: false,
      minLength: 12,
      retryEAGAIN: (err, writeBufferLen, remainingBufferLen) => {
        assert.strictEqual(err.code, 'EBUSY');
        assert.strictEqual(writeBufferLen, 12);
        assert.strictEqual(remainingBufferLen, 0);
        return false; // Don't retry
      }
    });

    await new Promise((resolve, reject) => {
      stream.on('ready', () => {
        stream.once('error', (err) => {
          assert.strictEqual(err.code, 'EBUSY');
          assert.ok(stream.write('something else\n'));
        });

        assert.ok(stream.write('hello world\n'));

        stream.end();

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
      });
    });
  } finally {
    // Restore original function
    fs.write = originalWrite;

    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch {
      // Ignore cleanup errors
    }
  }
});
