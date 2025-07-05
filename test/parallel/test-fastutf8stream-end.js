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

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`end after reopen - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, minLength: 4096, sync });

    await new Promise((resolve) => {
      stream.once('ready', () => {
        const after = dest + '-moved';
        stream.reopen(after);
        stream.write('after reopen\n');
        stream.on('finish', () => {
          fs.readFile(after, 'utf8', (err, data) => {
            assert.ifError(err);
            assert.strictEqual(data, 'after reopen\n');

            // Cleanup
            try {
              fs.unlinkSync(dest);
              fs.unlinkSync(after);
            } catch (err) {
              console.warn('Cleanup error:', err.message);
            }

            resolve();
          });
        });
        stream.end();
      });
    });
  });

  test(`end after 2x reopen - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, minLength: 4096, sync });

    await new Promise((resolve) => {
      stream.once('ready', () => {
        stream.reopen(dest + '-moved');
        const after = dest + '-moved-moved';
        stream.reopen(after);
        stream.write('after reopen\n');
        stream.on('finish', () => {
          fs.readFile(after, 'utf8', (err, data) => {
            assert.ifError(err);
            assert.strictEqual(data, 'after reopen\n');

            // Cleanup
            try {
              fs.unlinkSync(dest);
              fs.unlinkSync(dest + '-moved');
              fs.unlinkSync(after);
            } catch (err) {
              console.warn('Cleanup error:', err.message);
            }

            resolve();
          });
        });
        stream.end();
      });
    });
  });

  test(`end if not ready - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, minLength: 4096, sync });
    const after = dest + '-moved';

    stream.reopen(after);
    stream.write('after reopen\n');

    await new Promise((resolve) => {
      stream.on('finish', () => {
        fs.readFile(after, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data, 'after reopen\n');

          // Cleanup
          try {
            fs.unlinkSync(dest);
            fs.unlinkSync(after);
          } catch {
            // Ignore cleanup errors
          }

          resolve();
        });
      });

      stream.end();
    });
  });

  test(`large data write - sync: ${sync}`, async (t) => {
    const dest = getTempFile();
    const stream = new FastUtf8Stream({ dest, sync });
    const str = Buffer.alloc(10000).fill('a').toString();

    let totalWritten = 0;
    const writeData = () => {
      if (totalWritten >= 10000) {
        stream.end();
        return;
      }

      const chunk = str.slice(totalWritten, totalWritten + 1000);
      if (stream.write(chunk)) {
        totalWritten += chunk.length;
        setImmediate(writeData);
      } else {
        stream.once('drain', () => {
          totalWritten += chunk.length;
          setImmediate(writeData);
        });
      }
    };

    await new Promise((resolve) => {
      stream.on('finish', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data.length, 10000);
          assert.strictEqual(data, str);

          // Cleanup
          try {
            fs.unlinkSync(dest);
          } catch {
            // Ignore cleanup errors
          }

          resolve();
        });
      });

      writeData();
    });
  });
}
