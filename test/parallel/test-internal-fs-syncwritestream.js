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
  assert.strictEqual(stream.listenerCount('end'), 1);
}

// Verify constructing the instance with specified options.
{
  const stream = new SyncWriteStream(1, { autoClose: false });

  assert.strictEqual(stream.fd, 1);
  assert.strictEqual(stream.readable, false);
  assert.strictEqual(stream.autoClose, false);
  assert.strictEqual(stream.listenerCount('end'), 1);
}

// Verify that the file will be written synchronously.
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);
  const chunk = Buffer.from('foo');

  assert.strictEqual(stream._write(chunk, null, common.mustCall(1)), true);
  assert.strictEqual(fs.readFileSync(filename).equals(chunk), true);
}

// Verify that the stream will unset the fd after destroy().
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);

  stream.on('close', common.mustCall(3));

  assert.strictEqual(stream.destroy(), true);
  assert.strictEqual(stream.fd, null);
  assert.strictEqual(stream.destroy(), true);
  assert.strictEqual(stream.destroySoon(), true);
}

// Verify that the 'end' event listener will also destroy the stream.
{
  const fd = fs.openSync(filename, 'w');
  const stream = new SyncWriteStream(fd);

  assert.strictEqual(stream.fd, fd);

  stream.emit('end');
  assert.strictEqual(stream.fd, null);
}
