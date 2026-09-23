// Flags: --experimental-vfs
'use strict';

// watch / promises.watch / watchFile through RealFSProvider.

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

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

// watchFile / unwatchFile: the listener must be forwarded to the real fs
// watcher (not stubbed), fire on change, and be removable by identity.
{
  fs.writeFileSync(path.join(root, 'wf.txt'), 'a');
  const listener = common.mustCall();
  myVfs.watchFile('/wf.txt', { interval: 10, persistent: false }, listener);
  fs.writeFileSync(path.join(root, 'wf.txt'), 'b');
  setTimeout(() => {
    myVfs.unwatchFile('/wf.txt', listener);
  }, 50);
}
