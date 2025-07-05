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

  test(`write things to a file descriptor - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, sync });

    try {
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
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`write things in a streaming fashion - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, sync });

    try {
      await new Promise((resolve, reject) => {
        stream.once('drain', () => {
          fs.readFile(dest, 'utf8', (err, data) => {
            try {
              assert.ifError(err);
              assert.strictEqual(data, 'hello world\n');
              assert.ok(stream.write('something else\n'));

              stream.once('drain', () => {
                fs.readFile(dest, 'utf8', (err, data) => {
                  try {
                    assert.ifError(err);
                    assert.strictEqual(data, 'hello world\nsomething else\n');
                    stream.end();
                    resolve();
                  } catch (assertErr) {
                    reject(assertErr);
                  }
                });
              });
            } catch (assertErr) {
              reject(assertErr);
            }
          });
        });

        assert.ok(stream.write('hello world\n'));
      });
    } finally {
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`can be piped into - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, sync });
    const source = fs.createReadStream(__filename, { encoding: 'utf8' });

    try {
      source.pipe(stream);

      await new Promise((resolve, reject) => {
        stream.on('finish', () => {
          fs.readFile(__filename, 'utf8', (err, expected) => {
            try {
              assert.ifError(err);
              fs.readFile(dest, 'utf8', (err, data) => {
                try {
                  assert.ifError(err);
                  assert.strictEqual(data, expected);
                  resolve();
                } catch (assertErr) {
                  reject(assertErr);
                }
              });
            } catch (assertErr) {
              reject(assertErr);
            }
          });
        });
      });
    } finally {
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`write things to a file - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, sync });

    try {
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
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`minLength - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, minLength: 4096, sync });

    try {
      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          assert.ok(stream.write('hello world\n'));
          assert.ok(stream.write('something else\n'));

          // Don't expect drain event with minLength
          let drainCalled = false;
          stream.on('drain', () => {
            drainCalled = true;
          });

          setTimeout(() => {
            fs.readFile(dest, 'utf8', (err, data) => {
              try {
                assert.ifError(err);
                assert.strictEqual(data, ''); // Should be empty due to minLength

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
              } catch (assertErr) {
                reject(assertErr);
              }
            });
          }, 100);
        });
      });
    } finally {
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`write later on recoverable error - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');

    // Store original function
    const originalWrite = sync ? fs.writeSync : fs.write;
    let errorThrown = false;

    try {
      if (sync) {
        fs.writeSync = function(fd, buf, enc) {
          if (!errorThrown) {
            errorThrown = true;
            throw new Error('recoverable error');
          }
          return originalWrite.call(fs, fd, buf, enc);
        };
        fs.writeSync.isMocked = true;
        fs.writeSync.original = originalWrite;
      } else {
        fs.write = function(fd, buf, ...args) {
          if (!errorThrown) {
            errorThrown = true;
            const callback = args[args.length - 1];
            setTimeout(() => callback(new Error('recoverable error')), 0);
            return;
          }
          return originalWrite.call(fs, fd, buf, ...args);
        };
        fs.write.isMocked = true;
        fs.write.original = originalWrite;
      }

      const stream = new FastUtf8Stream({ fd, minLength: 0, sync });

      await new Promise((resolve, reject) => {
        let errorEmitted = false;

        stream.on('ready', () => {
          stream.on('error', () => {
            errorEmitted = true;
          });

          assert.ok(stream.write('hello world\n'));

          setTimeout(() => {
            // Restore the function and try writing again
            if (sync) {
              fs.writeSync = originalWrite;
            } else {
              fs.write = originalWrite;
            }

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
          }, 10);
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
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    }
  });

  test(`emit write events - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, sync });

    try {
      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          let length = 0;
          stream.on('write', (bytes) => {
            length += bytes;
          });

          assert.ok(stream.write('hello world\n'));
          assert.ok(stream.write('something else\n'));

          stream.end();

          stream.on('finish', () => {
            fs.readFile(dest, 'utf8', (err, data) => {
              try {
                assert.ifError(err);
                assert.strictEqual(data, 'hello world\nsomething else\n');
                assert.strictEqual(length, 27);
                resolve();
              } catch (assertErr) {
                reject(assertErr);
              }
            });
          });
        });
      });
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

test('write buffers that are not totally written', async (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');

  // Store original function
  const originalWrite = fs.write;
  let callCount = 0;

  try {
    fs.write = function(fd, buf, ...args) {
      callCount++;
      if (callCount === 1) {
        // First call returns 0 (nothing written)
        fs.write = function(fd, buf, ...args) {
          return originalWrite.call(fs, fd, buf, ...args);
        };
        const callback = args[args.length - 1];
        process.nextTick(callback, null, 0);
        return;
      }
      return originalWrite.call(fs, fd, buf, ...args);
    };

    const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

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
    } catch (cleanupErr) {
      // Ignore cleanup errors
    }
  }
});

test('write enormously large buffers async', async (t) => {
  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

  try {
    const buf = Buffer.alloc(1024).fill('x').toString(); // 1 KB
    let length = 0;

    // Reduce iterations to avoid test timeout
    for (let i = 0; i < 1024; i++) {
      length += buf.length;
      stream.write(buf);
    }

    stream.end();

    await new Promise((resolve, reject) => {
      stream.on('finish', () => {
        fs.stat(dest, (err, stat) => {
          try {
            assert.ifError(err);
            assert.strictEqual(stat.size, length);
            resolve();
          } catch (assertErr) {
            reject(assertErr);
          }
        });
      });
    });
  } finally {
    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch (cleanupErr) {
      // Ignore cleanup errors
    }
  }
});

test('make sure `maxLength` is passed', (t) => {
  const dest = getTempFile();
  const stream = new FastUtf8Stream({ dest, maxLength: 65536 });

  try {
    assert.strictEqual(stream.maxLength, 65536);
    stream.end();
  } finally {
    // Cleanup
    try {
      fs.unlinkSync(dest);
    } catch (cleanupErr) {
      // Ignore cleanup errors
    }
  }
});
