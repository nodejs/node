// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const SyncWriteStream = require('internal/fs/sync_write_stream');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filename = path.join(tmpdir.path, 'sync-write-stream.txt');

// Verify constructing the instance with default options.
{
  const stream = new SyncWriteStream(1);

  assert.strictEqual(stream.fd, 1);
  assert.strictEqual(stream.readable, false);
  assert.strictEqual(stream.autoClose, true);
}

// Verify constructing the instance with specified options.
{
  const stream = new SyncWriteStream(1, { autoClose: false });

  assert.strictEqual(stream.fd, 1);
  assert.strictEqual(stream.readable, false);
  assert.strictEqual(stream.autoClose, false);
}

// Verify that the file will be written synchronously.
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);
  const chunk = Buffer.from('foo');

  assert.strictEqual(stream._write(chunk, null, common.mustCall(1)), true);
  assert.strictEqual(fs.readFileSync(filename).equals(chunk), true);

  fs.closeSync(fd);
}

// Verify that the stream will unset the fd after destroy().
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);

  stream.on('close', common.mustCall());
  assert.strictEqual(stream.destroy(), stream);
  assert.strictEqual(stream.fd, null);
}

// Verify that the stream will unset the fd after destroySoon().
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);

  stream.on('close', common.mustCall());
  assert.strictEqual(stream.destroySoon(), stream);
  assert.strictEqual(stream.fd, null);
}

// Verify that calling end() will also destroy the stream.
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);

  assert.strictEqual(stream.fd, fd);

  stream.end();
  stream.on('close', common.mustCall(() => {
    assert.strictEqual(stream.fd, null);
  }));
}
