'use strict';

// VFS streams must expose bytesRead, bytesWritten, and pending.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// ReadStream: bytesRead and pending
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/stream.txt', 'stream data');

  const rs = myVfs.createReadStream('/stream.txt');
  assert.strictEqual(rs.pending, true);

  rs.on('data', common.mustCall(() => {
    assert.strictEqual(rs.pending, false);
    assert.ok(rs.bytesRead > 0);
  }));

  rs.on('end', common.mustCall());
}

// WriteStream: bytesWritten and pending
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/out.txt', '');

  const ws = myVfs.createWriteStream('/out.txt');
  assert.strictEqual(ws.pending, true);
  assert.strictEqual(ws.bytesWritten, 0);

  ws.write('hello', common.mustCall(() => {
    assert.strictEqual(ws.pending, false);
    assert.strictEqual(ws.bytesWritten, 5);
    ws.end();
  }));
}
