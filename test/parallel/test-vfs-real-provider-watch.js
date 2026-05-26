// Flags: --experimental-vfs
'use strict';

// watch / promises.watch / watchFile through RealFSProvider.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'real-provider-watch');
fs.mkdirSync(root, { recursive: true });
const myVfs = vfs.create(new vfs.RealFSProvider(root));

assert.strictEqual(myVfs.provider.supportsWatch, true);

// fs.watch wrapper
{
  fs.writeFileSync(path.join(root, 'watch-me.txt'), 'a');
  const watcher = myVfs.watch('/watch-me.txt', { persistent: false });
  watcher.close();
}

// promises.watch wrapper
(async () => {
  fs.writeFileSync(path.join(root, 'pwatch.txt'), 'a');
  const iter = myVfs.promises.watch('/pwatch.txt', { persistent: false });
  await iter.return();
})().then(common.mustCall());

// watchFile / unwatchFile
{
  fs.writeFileSync(path.join(root, 'wf.txt'), 'a');
  const listener = () => {};
  myVfs.watchFile('/wf.txt', { persistent: false }, listener);
  myVfs.unwatchFile('/wf.txt', listener);
}
