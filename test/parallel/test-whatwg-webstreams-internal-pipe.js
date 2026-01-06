// Flags: --expose-internals --no-warnings
'use strict';

// Tests for internal StreamBase pipe optimization (nodejs/performance#134)

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const {
  newWritableStreamFromStreamBase,
  newReadableStreamFromStreamBase,
} = require('internal/webstreams/adapters');

const { kStreamBase } = require('internal/webstreams/util');
const { JSStream } = internalBinding('js_stream');

// kStreamBase marker is attached to ReadableStream
{
  const stream = new JSStream();
  const readable = newReadableStreamFromStreamBase(stream);
  assert.strictEqual(readable[kStreamBase], stream);
  stream.emitEOF();
}

// kStreamBase marker is attached to WritableStream
{
  const stream = new JSStream();
  stream.onwrite = common.mustNotCall();
  stream.onshutdown = (req) => req.oncomplete();

  const writable = newWritableStreamFromStreamBase(stream);
  assert.strictEqual(writable[kStreamBase], stream);
  writable.close();
}

// Regular JS streams don't have kStreamBase
{
  const { ReadableStream, WritableStream } = require('stream/web');

  const rs = new ReadableStream({
    pull(controller) {
      controller.enqueue('chunk');
      controller.close();
    },
  });

  const ws = new WritableStream({ write() {} });

  assert.strictEqual(rs[kStreamBase], undefined);
  assert.strictEqual(ws[kStreamBase], undefined);

  rs.pipeTo(ws).then(common.mustCall());
}

// Mixed streams use standard JS path
{
  const stream = new JSStream();
  stream.onshutdown = (req) => req.oncomplete();
  const readable = newReadableStreamFromStreamBase(stream);

  const { WritableStream } = require('stream/web');
  const chunks = [];
  const ws = new WritableStream({
    write(chunk) { chunks.push(chunk); },
  });

  assert.ok(readable[kStreamBase]);
  assert.strictEqual(ws[kStreamBase], undefined);

  const pipePromise = readable.pipeTo(ws);
  stream.readBuffer(Buffer.from('hello'));
  stream.emitEOF();

  pipePromise.then(common.mustCall(() => {
    assert.strictEqual(chunks.length, 1);
  }));
}

// Verify kStreamBase symbol identity
{
  const { kStreamBase: kStreamBase2 } = require('internal/webstreams/util');
  assert.strictEqual(kStreamBase, kStreamBase2);
}

// FileHandle.readableWebStream() uses async reads, not StreamBase
{
  const fs = require('fs/promises');
  const path = require('path');
  const os = require('os');

  async function testFileStreamPipe() {
    const tmpDir = os.tmpdir();
    const testFile = path.join(tmpDir, `test-webstream-pipe-${process.pid}.txt`);
    const testData = 'Hello, WebStreams pipe!';

    await fs.writeFile(testFile, testData);

    try {
      const fileHandle = await fs.open(testFile, 'r');
      const readable = fileHandle.readableWebStream();

      assert.strictEqual(readable[kStreamBase], undefined);

      const chunks = [];
      const writable = new (require('stream/web').WritableStream)({
        write(chunk) { chunks.push(chunk); },
      });

      await readable.pipeTo(writable);
      await fileHandle.close();

      const result = Buffer.concat(chunks.map((c) =>
        (c instanceof Uint8Array ? Buffer.from(c) : Buffer.from(c)),
      )).toString();
      assert.strictEqual(result, testData);
    } finally {
      await fs.unlink(testFile).catch(() => {});
    }
  }

  testFileStreamPipe().then(common.mustCall());
}
