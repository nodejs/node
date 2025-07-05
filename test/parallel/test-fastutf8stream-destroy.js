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

test('FastUtf8Stream destroy - basic functionality', async (t) => {
  // Reset the umask for testing
  process.umask(0o000);

  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  // Test successful write
  const writeResult = stream.write('hello world\n');
  assert.ok(writeResult);
  
  // Destroy the stream
  stream.destroy();
  
  // Test that write throws after destroy
  assert.throws(() => { 
    stream.write('hello world\n'); 
  });

  // Wait for file to be written and check content
  await new Promise((resolve, reject) => {
    fs.readFile(dest, 'utf8', (err, data) => {
      if (err) reject(err);
      else {
        assert.strictEqual(data, 'hello world\n');
        resolve();
      }
    });
  });

  // Test events - use Promise to handle async events
  const eventPromises = [];
  
  // finish event should NOT be emitted after destroy
  eventPromises.push(new Promise((resolve) => {
    const finishTimeout = setTimeout(() => {
      resolve('finish not emitted'); // This is what we want
    }, 100);
    
    stream.on('finish', () => {
      clearTimeout(finishTimeout);
      resolve('finish emitted');
    });
  }));

  // close event SHOULD be emitted after destroy
  eventPromises.push(new Promise((resolve) => {
    stream.on('close', () => {
      resolve('close emitted');
    });
  }));

  const [finishResult, closeResult] = await Promise.all(eventPromises);
  
  assert.strictEqual(finishResult, 'finish not emitted');
  assert.strictEqual(closeResult, 'close emitted');

  // Cleanup
  try {
    fs.unlinkSync(dest);
  } catch (err) {
    // Ignore cleanup errors
  }
});

test('FastUtf8Stream destroy - sync mode', async (t) => {
  process.umask(0o000);

  const dest = getTempFile();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  assert.ok(stream.write('hello world\n'));
  stream.destroy();
  assert.throws(() => { stream.write('hello world\n'); });

  const data = fs.readFileSync(dest, 'utf8');
  assert.strictEqual(data, 'hello world\n');

  // Cleanup
  try {
    fs.unlinkSync(dest);
  } catch (err) {
    // Ignore cleanup errors
  }
});

test('FastUtf8Stream destroy while opening', async (t) => {
  const dest = getTempFile();
  const stream = new FastUtf8Stream({ dest });

  // Destroy immediately while opening
  stream.destroy();
  
  // Wait for close event
  await new Promise((resolve) => {
    stream.on('close', () => {
      resolve();
    });
  });

  // Cleanup
  try {
    fs.unlinkSync(dest);
  } catch (err) {
    // Ignore cleanup errors
  }
});
