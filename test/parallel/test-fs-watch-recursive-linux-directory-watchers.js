'use strict';
const common = require('../common');

// This tests the JavaScript recursive fs.watch() implementation (used where
// there is no native one, e.g. Linux): handle count, event reporting, ref/unref.

if (!common.isLinux)
  common.skip('the recursive watcher is native on this platform');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { setTimeout: wait } = require('timers/promises');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const delay = () => wait(common.platformTimeout(100));

function countFSEventHandles() {
  return process.getActiveResourcesInfo().filter((type) => type === 'FSEventWrap').length;
}

// Collects events until `done(events)` returns true, then closes the watcher.
function watchUntil(target, done) {
  return new Promise((resolve) => {
    const events = [];
    const watcher = fs.watch(target, { recursive: true });
    watcher.on('change', (eventType, filename) => {
      events.push(`${eventType} ${filename}`);
      if (done(events)) {
        watcher.close();
        resolve(events);
      }
    });
  });
}

(async () => {
  const root = tmpdir.resolve('root');
  fs.mkdirSync(path.join(root, 'sub'), { recursive: true });
  for (let i = 0; i < 20; i++) {
    fs.writeFileSync(path.join(root, `file-${i}.txt`), 'x');
    fs.writeFileSync(path.join(root, 'sub', `file-${i}.txt`), 'x');
  }
  fs.symlinkSync(path.join(root, 'sub', 'file-1.txt'), path.join(root, 'link'));

  {
    // One handle per directory (root, sub) plus one for the symbolic link.
    const before = countFSEventHandles();
    const watcher = fs.watch(root, { recursive: true });
    assert.strictEqual(countFSEventHandles() - before, 3);
    watcher.unref();
    watcher.ref();
    watcher.close();
  }

  {
    // A file replaced by rename() keeps being reported.
    const file = path.join(root, 'sub', 'file-0.txt');
    const changed = `change ${path.join('sub', 'file-0.txt')}`;
    const events = watchUntil(root, (seen) => seen.filter((e) => e === changed).length === 2);
    await delay();
    fs.writeFileSync(`${file}.tmp`, 'replaced');
    fs.renameSync(`${file}.tmp`, file);
    await delay();
    fs.appendFileSync(file, ' and modified');
    await events;
  }

  {
    // A write behind a symbolic link is reported as a rename of the link, and
    // touching a known subdirectory itself does not throw or emit.
    const events = watchUntil(root, (seen) => seen.includes('rename link'));
    await delay();
    fs.chmodSync(path.join(root, 'sub'), 0o775);
    fs.appendFileSync(path.join(root, 'sub', 'file-1.txt'), 'more');
    assert.ok(!(await events).some((e) => e.endsWith(' sub')));
  }

  {
    // Removing the watched root directory is reported.
    const doomed = tmpdir.resolve('doomed');
    fs.mkdirSync(doomed);
    fs.writeFileSync(path.join(doomed, 'inside'), 'x');
    const events = watchUntil(doomed, (seen) => seen.includes('rename '));
    await delay();
    fs.rmSync(doomed, { recursive: true });
    await events;
  }

  {
    // So is removing a watched root that is a file (with an empty filename,
    // as before).
    const lone = tmpdir.resolve('lone.txt');
    fs.writeFileSync(lone, 'x');
    const events = watchUntil(lone, (seen) => seen.includes('rename '));
    await delay();
    fs.rmSync(lone);
    await events;
  }

  {
    const watcher = fs.watch(root, { recursive: true });
    watcher.unref();
    // The process exits although this watcher is never closed.
    process.on('exit', () => assert.ok(watcher));
  }
})().then(common.mustCall());
