'use strict';

// Tests for VFS stream options:
// - createWriteStream({ start }) writes at the correct offset
// - createReadStream({ fd }) uses the provided fd
// - createWriteStream({ fd, start }) uses the provided fd and offset

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// createWriteStream({ start: 3 }) writes at byte offset 3.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abcdef');
  myVfs.mount('/mnt-ws-start');

  const ws = fs.createWriteStream('/mnt-ws-start/file.txt', {
    flags: 'r+',
    start: 3,
  });
  ws.end('Z');
  ws.on('close', common.mustCall(() => {
    assert.strictEqual(
      fs.readFileSync('/mnt-ws-start/file.txt', 'utf8'), 'abcZef');
    myVfs.unmount();
  }));
}

// createReadStream with fd option uses the provided fd instead of opening
// the path.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abcdef');
  myVfs.mount('/mnt-rsfd');

  const fd = fs.openSync('/mnt-rsfd/file.txt', 'r');
  const chunks = [];
  const rs = fs.createReadStream('/mnt-rsfd/nonexistent', {
    fd,
    autoClose: false,
  });
  rs.on('data', (chunk) => chunks.push(chunk));
  rs.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), 'abcdef');
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}

// createWriteStream with fd option uses the provided fd and start offset.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abcdef');
  myVfs.mount('/mnt-wsfd');

  const fd = fs.openSync('/mnt-wsfd/file.txt', 'r+');
  const ws = fs.createWriteStream('/mnt-wsfd/other.txt', {
    fd,
    autoClose: false,
    start: 3,
  });
  ws.end('Z');
  ws.on('close', common.mustCall(() => {
    assert.strictEqual(fs.readFileSync('/mnt-wsfd/file.txt', 'utf8'), 'abcZef');
    fs.closeSync(fd);
    myVfs.unmount();
  }));
}
