// Flags: --disable-warning=ExperimentalWarning
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
  // Unwatch on the first call because writeFileSync truncates before writing,
  // so a poll landing between the two would report a second change. The
  // watcher is persistent, so the process only exits once it has been removed.
  const listener = common.mustCall(() => {
    myVfs.unwatchFile('/wf.txt', listener);
  });
  myVfs.watchFile('/wf.txt', { interval: 10 }, listener);
  // uv_fs_poll records the first stat asynchronously as the baseline and
  // only reports subsequent changes, so mutate the file only after that
  // baseline reflects 'a'; otherwise the change is folded into the baseline
  // and the listener never fires. Change the size too, because some
  // platforms (e.g. AIX) only report timestamps to the second, so a
  // same-size rewrite within that second would look unchanged.
  setTimeout(() => fs.writeFileSync(path.join(root, 'wf.txt'), 'bb'), 100);
}

// watchFile listener fires when the underlying file changes.
// Refs: https://github.com/nodejs/node/issues/64536
{
  const target = path.join(root, 'wf-change.txt');
  fs.writeFileSync(target, 'a');
  let size = 2;
  const rewrite = setInterval(() => {
    fs.writeFileSync(target, 'x'.repeat(size++));
  }, 100);
  myVfs.watchFile('/wf-change.txt', { interval: 50 }, common.mustCall((curr, prev) => {
    clearInterval(rewrite);
    myVfs.unwatchFile('/wf-change.txt');
    assert.notStrictEqual(curr.size, prev.size);
  }));
}
