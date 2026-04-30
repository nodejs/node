'use strict';

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test that writeFileSync updates both mtime and ctime
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'initial');

  const stat1 = myVfs.statSync('/file.txt');
  const oldCtime = stat1.ctimeMs;

  myVfs.writeFileSync('/file.txt', 'updated');
  const stat2 = myVfs.statSync('/file.txt');
  assert.ok(stat2.mtimeMs >= oldCtime);
  assert.ok(stat2.ctimeMs >= oldCtime);
  // Ctime and mtime should be the same value (both set from same write)
  assert.strictEqual(stat2.ctimeMs, stat2.mtimeMs);
}

// Test that writeSync via file handle updates ctime
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'initial');

  const fd = myVfs.openSync('/file.txt', 'r+');
  const buf = Buffer.from('X');
  myVfs.writeSync(fd, buf, 0, 1, 0);
  myVfs.closeSync(fd);

  const stat = myVfs.statSync('/file.txt');
  assert.strictEqual(stat.ctimeMs, stat.mtimeMs);
}

// Test that truncateSync updates ctime
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'some content');

  const fd = myVfs.openSync('/file.txt', 'r+');
  myVfs.ftruncateSync(fd, 0);
  myVfs.closeSync(fd);

  const stat = myVfs.statSync('/file.txt');
  assert.strictEqual(stat.ctimeMs, stat.mtimeMs);
}
