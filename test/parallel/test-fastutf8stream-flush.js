'use strict';

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

function runTests(buildTests) {
  buildTests(test, false);
  buildTests(test, true);
}

runTests(buildTests);

function buildTests(test, sync) {
  // Reset the umask for testing
  process.umask(0o000);

  test(`append - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    fs.writeFileSync(dest, 'hello world\n');
    const stream = new FastUtf8Stream({ dest, append: false, sync });

    stream.on('ready', () => {
      assert.ok(stream.write('something else\n'));
      stream.flush();

      stream.on('drain', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data, 'something else\n');
          stream.end();
          
          // Cleanup
          try {
            fs.unlinkSync(dest);
          } catch (cleanupErr) {
            // Ignore cleanup errors
          }
        });
      });
    });
  });

  test(`mkdir - sync: ${sync}`, (t) => {
    const dest = path.join(getTempFile(), 'out.log');
    const stream = new FastUtf8Stream({ dest, mkdir: true, sync });

    stream.on('ready', () => {
      assert.ok(stream.write('hello world\n'));
      stream.flush();

      stream.on('drain', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data, 'hello world\n');
          stream.end();
          
          // Cleanup
          try {
            fs.unlinkSync(dest);
            fs.rmdirSync(path.dirname(dest));
          } catch (cleanupErr) {
            // Ignore cleanup errors
          }
        });
      });
    });
  });

  test(`flush - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', () => {
      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));
      stream.flush();

      stream.on('drain', () => {
        fs.readFile(dest, 'utf8', (err, data) => {
          assert.ifError(err);
          assert.strictEqual(data, 'hello world\nsomething else\n');
          stream.end();
          
          // Cleanup
          try {
            fs.unlinkSync(dest);
          } catch (cleanupErr) {
            // Ignore cleanup errors
          }
        });
      });
    });
  });

  test(`flush with no data - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', () => {
      stream.flush();

      stream.on('drain', () => {
        stream.end();
        
        // Cleanup
        try {
          fs.unlinkSync(dest);
        } catch (cleanupErr) {
          // Ignore cleanup errors
        }
      });
    });
  });

  test(`call flush cb after flushed - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', () => {
      assert.ok(stream.write('hello world\n'));
      assert.ok(stream.write('something else\n'));

      stream.flush((err) => {
        assert.ifError(err);
        stream.end();
        
        // Cleanup
        try {
          fs.unlinkSync(dest);
        } catch (cleanupErr) {
          // Ignore cleanup errors
        }
      });
    });
  });

  test(`call flush cb even when have no data - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', () => {
      stream.flush((err) => {
        assert.ifError(err);
        stream.end();
        
        // Cleanup
        try {
          fs.unlinkSync(dest);
        } catch (cleanupErr) {
          // Ignore cleanup errors
        }
      });
    });
  });

  test(`call flush cb even when minLength is 0 - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 0, sync });

    stream.flush((err) => {
      assert.ifError(err);
      stream.end();
      
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    });
  });

  test(`call flush cb with an error when trying to flush destroyed stream - sync: ${sync}`, (t) => {
    const dest = getTempFile();
    const fd = fs.openSync(dest, 'w');
    const stream = new FastUtf8Stream({ fd, minLength: 4096, sync });
    stream.destroy();

    stream.flush((err) => {
      assert.ok(err);
      
      // Cleanup
      try {
        fs.unlinkSync(dest);
      } catch (cleanupErr) {
        // Ignore cleanup errors
      }
    });
  });
}