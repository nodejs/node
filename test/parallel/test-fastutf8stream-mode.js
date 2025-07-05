'use strict';

require('../common');
const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { FastUtf8Stream } = require('node:fs');
const { tmpdir } = require('node:os');

let fileCounter = 0;

function getTempFile() {
  return path.join(tmpdir(), `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

const isWindows = process.platform === 'win32';

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`mode - sync: ${sync}`, { skip: isWindows }, async (t) => {
    const dest = getTempFile();
    const mode = 0o666;
    const stream = new FastUtf8Stream({ dest, sync, mode });

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
                assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
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
      } catch (err) {
        console.warn('Cleanup error:', err.message);
      }
    }
  });

  test(`mode default - sync: ${sync}`, { skip: isWindows }, async (t) => {
    const dest = getTempFile();
    const defaultMode = 0o666;
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
                assert.strictEqual(fs.statSync(dest).mode & 0o777, defaultMode);
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
      } catch (err) {
        console.warn('Cleanup error:', err.message);
      }
    }
  });

  test(`mode on mkdir - sync: ${sync}`, { skip: isWindows }, async (t) => {
    const dest = path.join(getTempFile(), 'out.log');
    const mode = 0o666;
    const stream = new FastUtf8Stream({ dest, mkdir: true, mode, sync });

    try {
      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          assert.ok(stream.write('hello world\n'));

          stream.flush();

          stream.on('drain', () => {
            fs.readFile(dest, 'utf8', (err, data) => {
              try {
                assert.ifError(err);
                assert.strictEqual(data, 'hello world\n');
                assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
                stream.end();
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
        fs.rmdirSync(path.dirname(dest));
      } catch (err) {
        console.warn('Cleanup error:', err.message);
      }
    }
  });

  test(`mode on append - sync: ${sync}`, { skip: isWindows }, async (t) => {
    const dest = getTempFile();
    // Create file with writable mode first, then change mode after FastUtf8Stream creation
    fs.writeFileSync(dest, 'hello world\n', { encoding: 'utf8' });
    const mode = isWindows ? 0o444 : 0o666;
    const stream = new FastUtf8Stream({ dest, append: false, mode, sync });

    try {
      await new Promise((resolve, reject) => {
        stream.on('ready', () => {
          assert.ok(stream.write('something else\n'));

          stream.flush();

          stream.on('drain', () => {
            fs.readFile(dest, 'utf8', (err, data) => {
              try {
                assert.ifError(err);
                assert.strictEqual(data, 'something else\n');
                assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
                stream.end();
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
      } catch (err) {
        console.warn('Cleanup error:', err.message);
      }
    }
  });
}
