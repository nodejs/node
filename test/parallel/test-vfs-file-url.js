'use strict';

// Verify mounted VFS correctly handles file: URLs.
// Previously, URL inputs used path.pathname instead of fileURLToPath(),
// which left percent-encoded characters in place.

require('../common');
const assert = require('assert');
const fs = require('fs');
const { pathToFileURL } = require('url');
const vfs = require('node:vfs');

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/hello world.txt', 'ok');
  myVfs.mount('/mnt');

  const url = pathToFileURL('/mnt/hello world.txt');
  assert.strictEqual(fs.readFileSync(url, 'utf8'), 'ok');
  assert.strictEqual(fs.existsSync(url), true);
  const st = fs.statSync(url);
  assert.strictEqual(st.isFile(), true);

  myVfs.unmount();
}
