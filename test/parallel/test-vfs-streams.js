// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test basic createReadStream
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'hello world');

  const stream = myVfs.createReadStream('/file.txt');
  let data = '';

  stream.on('open', common.mustCall((fd) => {
    assert.notStrictEqual(fd & 0x40000000, 0);
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
  const myVfs = vfs.create();
  myVfs.writeFileSync('/encoded.txt', 'encoded content');

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
  const myVfs = vfs.create();
  myVfs.writeFileSync('/range.txt', '0123456789');

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
  const myVfs = vfs.create();
  myVfs.writeFileSync('/start.txt', 'abcdefghij');

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
  const myVfs = vfs.create();

  const stream = myVfs.createReadStream('/nonexistent.txt');

  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

// Test createReadStream path property
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/path-test.txt', 'test');

  const stream = myVfs.createReadStream('/path-test.txt');
  assert.strictEqual(stream.path, '/path-test.txt');

  stream.on('data', () => {}); // Consume data
  stream.on('end', common.mustCall());
}

// Test createReadStream with small highWaterMark
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/small-hwm.txt', 'AAAABBBBCCCCDDDD');

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
  const myVfs = vfs.create();
  myVfs.writeFileSync('/destroy.txt', 'content to destroy');

  const stream = myVfs.createReadStream('/destroy.txt');

  stream.on('open', common.mustCall(() => {
    stream.destroy();
  }));

  stream.on('close', common.mustCall());
}

// Test createReadStream with large file
{
  const myVfs = vfs.create();
  const largeContent = 'X'.repeat(100000);
  myVfs.writeFileSync('/large.txt', largeContent);

  const stream = myVfs.createReadStream('/large.txt');
  let receivedLength = 0;

  stream.on('data', (chunk) => {
    receivedLength += chunk.length;
  });

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(receivedLength, 100000);
  }));
}

// Test createReadStream pipe to another stream
{
  const myVfs = vfs.create();
  const { Writable } = require('stream');

  myVfs.writeFileSync('/pipe-source.txt', 'pipe this content');

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
  const myVfs = vfs.create();
  myVfs.writeFileSync('/no-auto-close.txt', 'content');

  const stream = myVfs.createReadStream('/no-auto-close.txt', {
    autoClose: false,
  });

  stream.on('close', common.mustCall());

  // Start flowing the stream
  stream.resume();
  stream.on('end', common.mustCall(() => {
    // With autoClose: false, close should not be emitted automatically
    // We need to manually destroy the stream
    setImmediate(() => {
      stream.destroy();
    });
  }));
}

// ==================== Additional coverage ====================

const { Readable } = require('stream');
const { pipeline } = require('stream/promises');

// Slicing read stream with start/end
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/slice.txt', 'hello world');
  const rs = myVfs.createReadStream('/slice.txt', { start: 6, end: 10 });
  const chunks = [];
  rs.on('data', (c) => chunks.push(c));
  rs.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), 'world');
  }));
}

// createReadStream via for-await iteration
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/iter.txt', 'abc');
  const rs = myVfs.createReadStream('/iter.txt', { encoding: 'utf8' });
  const chunks = [];
  for await (const chunk of rs) {
    chunks.push(chunk);
  }
  assert.strictEqual(chunks.join(''), 'abc');
})().then(common.mustCall());

// start: beyond file size → empty stream
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/sm.txt', 'abc');
  const rs = myVfs.createReadStream('/sm.txt', { start: 10 });
  rs.on('data', common.mustNotCall('no data expected'));
  rs.on('end', common.mustCall());
}

// Empty file → end immediately
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/empty.txt', '');
  const rs = myVfs.createReadStream('/empty.txt');
  rs.on('data', common.mustNotCall('no data expected'));
  rs.on('end', common.mustCall());
}

// Pipeline write
(async () => {
  const myVfs = vfs.create();
  await pipeline(
    Readable.from([Buffer.from('hello'), Buffer.from(' world')]),
    myVfs.createWriteStream('/out.txt'),
  );
  assert.strictEqual(myVfs.readFileSync('/out.txt', 'utf8'), 'hello world');
})().then(common.mustCall());

// Pipeline read
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/in.txt', 'hello world');
  await pipeline(
    myVfs.createReadStream('/in.txt'),
    myVfs.createWriteStream('/copied.txt'),
  );
  assert.strictEqual(myVfs.readFileSync('/copied.txt', 'utf8'), 'hello world');
})().then(common.mustCall());

// Pipeline write with start position
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/pad.txt', 'AAAAAAAAAA');
  await pipeline(
    Readable.from([Buffer.from('XX')]),
    myVfs.createWriteStream('/pad.txt', { start: 3, flags: 'r+' }),
  );
  assert.strictEqual(myVfs.readFileSync('/pad.txt', 'utf8'), 'AAAXXAAAAA');
})().then(common.mustCall());

// Write string chunk + encoding callback
(async () => {
  const myVfs = vfs.create();
  const stream = myVfs.createWriteStream('/str.txt');
  await new Promise((resolve, reject) => {
    stream.write('hello', 'utf8', (err) => (err ? reject(err) : resolve()));
  });
  await new Promise((resolve) => stream.end(resolve));
  assert.strictEqual(myVfs.readFileSync('/str.txt', 'utf8'), 'hello');
})().then(common.mustCall());

// path getter
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/p.txt', 'x');
  const rs = myVfs.createReadStream('/p.txt');
  assert.strictEqual(rs.path, '/p.txt');
  rs.destroy();

  const ws = myVfs.createWriteStream('/p2.txt');
  assert.strictEqual(ws.path, '/p2.txt');
  ws.destroy();
}

// destroy() before any data triggers _destroy + close cleanup
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/d.txt', 'data');
  const rs = myVfs.createReadStream('/d.txt');
  rs.on('error', () => {});
  rs.destroy();
}
