'use strict';

// fsPromises.open() must be intercepted for VFS paths.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/hello.txt', 'hello from vfs');
myVfs.mount('/mnt_popen');

(async () => {
  const fh = await fs.promises.open('/mnt_popen/hello.txt', 'r');
  assert.ok(fh);
  const content = await fh.readFile('utf8');
  assert.strictEqual(content, 'hello from vfs');
  await fh.close();
  myVfs.unmount();
})().then(common.mustCall());
