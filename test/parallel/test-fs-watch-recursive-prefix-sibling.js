'use strict';

// Regression test for https://github.com/nodejs/node/issues/58868

const common = require('../common');

if (common.isIBMi) {
  common.skip('IBMi does not support `fs.watch()`');
}

if (common.isAIX) {
  common.skip('folder watch capability is limited in AIX.');
}

// macOS and Windows use the native recursive watcher and are unaffected.
if (common.isMacOS || common.isWindows) {
  common.skip('regression specific to the JS-based recursive watcher');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { setTimeout } = require('timers/promises');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

(async () => {
  const root = fs.mkdtempSync(path.join(tmpdir.path, 'watch-prefix-'));

  // Sibling names that share the prefix `foo` with the entries to delete.
  fs.mkdirSync(path.join(root, 'foo_bar'));
  fs.writeFileSync(path.join(root, 'foo_bar', 'file.txt'), '');
  fs.mkdirSync(path.join(root, 'foo_bar', 'somedir'));
  fs.writeFileSync(path.join(root, 'foo_'), '');

  // `foo` (empty) exercises the exact-match branch of `#unwatchFiles`.
  fs.mkdirSync(path.join(root, 'foo'));

  // `foo2` has descendants and exercises the `file + sep` prefix branch.
  fs.mkdirSync(path.join(root, 'foo2'));
  fs.writeFileSync(path.join(root, 'foo2', 'inside.txt'), '');
  fs.mkdirSync(path.join(root, 'foo2', 'sub'));

  const events = [];
  const watcher = fs.watch(root, { recursive: true }, (eventType, filename) => {
    events.push({ eventType, filename });
  });

  // Allow the watcher to fully attach to existing entries.
  await setTimeout(common.platformTimeout(200));

  fs.rmdirSync(path.join(root, 'foo'));
  fs.rmSync(path.join(root, 'foo2'), { recursive: true });

  // Wait long enough to capture any spurious follow-up events.
  await setTimeout(common.platformTimeout(500));

  watcher.close();

  const isSibling = (f) =>
    f === 'foo_' || f === 'foo_bar' ||
    f.startsWith('foo_bar' + path.sep);
  const spurious = events.filter((e) => isSibling(e.filename));
  assert.deepStrictEqual(
    spurious,
    [],
    `unexpected events for prefix-sibling entries: ${JSON.stringify(spurious)}`,
  );
})().then(common.mustCall());
