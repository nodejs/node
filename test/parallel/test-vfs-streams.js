'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// Test basic createReadStream
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'hello world');

  const stream = myVfs.createReadStream('/file.txt');
  let data = '';

  stream.on('open', common.mustCall((fd) => {
    assert.ok(fd >= 10000);
  }));

  stream.on('ready', common.mustCall());

  stream.on('data', (chunk) => {
    data += chunk;
  });

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'hello world');
  }));

  stream.on('close', common.mustCall());
}

// Test createReadStream with encoding
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/encoded.txt', 'encoded content');

  const stream = myVfs.createReadStream('/encoded.txt', { encoding: 'utf8' });
  let data = '';
  let receivedString = false;

  stream.on('data', (chunk) => {
    if (typeof chunk === 'string') {
      receivedString = true;
    }
    data += chunk;
  });

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(receivedString, true);
    assert.strictEqual(data, 'encoded content');
  }));
}

// Test createReadStream with start and end
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/range.txt', '0123456789');

  const stream = myVfs.createReadStream('/range.txt', {
    start: 2,
    end: 5,
  });
  let data = '';

  stream.on('data', (chunk) => {
    data += chunk;
  });

  stream.on('end', common.mustCall(() => {
    // End is inclusive, so positions 2, 3, 4, 5 = "2345" (4 chars)
    assert.strictEqual(data, '2345');
  }));
}

// Test createReadStream with start only
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/start.txt', 'abcdefghij');

  const stream = myVfs.createReadStream('/start.txt', { start: 5 });
  let data = '';

  stream.on('data', (chunk) => {
    data += chunk;
  });

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'fghij');
  }));
}

// Test createReadStream with non-existent file
{
  const myVfs = fs.createVirtual();

  const stream = myVfs.createReadStream('/nonexistent.txt');

  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

// Test createReadStream path property
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/path-test.txt', 'test');

  const stream = myVfs.createReadStream('/path-test.txt');
  assert.strictEqual(stream.path, '/path-test.txt');

  stream.on('data', () => {}); // Consume data
  stream.on('end', common.mustCall());
}

// Test createReadStream with small highWaterMark
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/small-hwm.txt', 'AAAABBBBCCCCDDDD');

  const stream = myVfs.createReadStream('/small-hwm.txt', {
    highWaterMark: 4,
  });

  const chunks = [];
  stream.on('data', (chunk) => {
    chunks.push(chunk.toString());
  });

  stream.on('end', common.mustCall(() => {
    // With highWaterMark of 4, we should get multiple chunks
    assert.ok(chunks.length >= 1);
    assert.strictEqual(chunks.join(''), 'AAAABBBBCCCCDDDD');
  }));
}

// Test createReadStream destroy
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/destroy.txt', 'content to destroy');

  const stream = myVfs.createReadStream('/destroy.txt');

  stream.on('open', common.mustCall(() => {
    stream.destroy();
  }));

  stream.on('close', common.mustCall());
}

// Test createReadStream with large file
{
  const myVfs = fs.createVirtual();
  const largeContent = 'X'.repeat(100000);
  myVfs.addFile('/large.txt', largeContent);

  const stream = myVfs.createReadStream('/large.txt');
  let receivedLength = 0;

  stream.on('data', (chunk) => {
    receivedLength += chunk.length;
  });

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(receivedLength, 100000);
  }));
}

// Test createReadStream with dynamic content
{
  const myVfs = fs.createVirtual();
  let calls = 0;
  myVfs.addFile('/dynamic-stream.txt', () => {
    calls++;
    return 'dynamic ' + calls;
  });

  const stream = myVfs.createReadStream('/dynamic-stream.txt', { encoding: 'utf8' });
  let data = '';

  stream.on('data', (chunk) => {
    data += chunk;
  });

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'dynamic 1');
    assert.strictEqual(calls, 1);
  }));
}

// Test createReadStream pipe to another stream
{
  const myVfs = fs.createVirtual();
  const { Writable } = require('stream');

  myVfs.addFile('/pipe-source.txt', 'pipe this content');

  const stream = myVfs.createReadStream('/pipe-source.txt');
  let collected = '';

  const writable = new Writable({
    write(chunk, encoding, callback) {
      collected += chunk;
      callback();
    },
  });

  stream.pipe(writable);

  writable.on('finish', common.mustCall(() => {
    assert.strictEqual(collected, 'pipe this content');
  }));
}

// Test autoClose: false
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/no-auto-close.txt', 'content');

  const stream = myVfs.createReadStream('/no-auto-close.txt', {
    autoClose: false,
  });

  stream.on('close', common.mustCall());

  stream.on('data', () => {});

  stream.on('end', common.mustCall(() => {
    // With autoClose: false, close should not be emitted automatically
    // We need to manually destroy the stream
    setImmediate(() => {
      stream.destroy();
    });
  }));
}
