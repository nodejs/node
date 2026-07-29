// Flags: --experimental-vfs
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const { pathToFileURL } = require('url');
const vfs = require('node:vfs');

// Test: null while not mounted.
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.mountPointURL, null);
}

// Test: file: URL of the mount point while mounted, null again after
// unmounting.
{
  const myVfs = vfs.create();
  const mountPoint = myVfs.mount();

  const url = myVfs.mountPointURL;
  assert.ok(url instanceof URL);
  assert.strictEqual(url.protocol, 'file:');
  assert.strictEqual(url.href, pathToFileURL(mountPoint).href);

  myVfs.unmount();
  assert.strictEqual(myVfs.mountPointURL, null);
}

// Test: each access returns a fresh URL object, so callers mutating the
// result cannot corrupt the instance state.
{
  const myVfs = vfs.create();
  myVfs.mount();

  const first = myVfs.mountPointURL;
  first.pathname += '/tampered';
  const second = myVfs.mountPointURL;
  assert.notStrictEqual(first, second);
  assert.strictEqual(second.href, pathToFileURL(myVfs.mountPoint).href);

  myVfs.unmount();
}

// Test: the URL is usable to address files in the mounted VFS.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'hello url');
  myVfs.mount();

  const fileURL = new URL(`${myVfs.mountPointURL}/data.txt`);
  assert.strictEqual(fs.readFileSync(fileURL, 'utf8'), 'hello url');

  myVfs.unmount();
}
