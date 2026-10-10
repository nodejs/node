'use strict';

const common = require('../common');

if (!common.isLinux)
  common.skip('the recursive watcher is native on this platform');

const assert = require('assert');
const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

(async () => {
  const filename = 'root-file.txt';
  const file = tmpdir.resolve(filename);
  fs.writeFileSync(file, 'content');

  const watcher = fsPromises.watch(file, { recursive: true });
  const event = watcher.next();

  process.nextTick(common.mustCall(() => fs.rmSync(file)));

  try {
    const { value, done } = await event;
    assert.strictEqual(done, false);
    assert.strictEqual(value.eventType, 'rename');
    assert.strictEqual(value.filename, path.basename(file));
  } finally {
    await watcher.return();
  }
})().then(common.mustCall());
