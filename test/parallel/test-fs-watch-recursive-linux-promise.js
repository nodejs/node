'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const { randomUUID } = require('crypto');
const assert = require('assert');
const path = require('path');
const fs = require('fs/promises');
const fsSync = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

(async function run() {
  // Add a file to already watching folder

  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const file = `${randomUUID()}.txt`;
  const filePath = path.join(testsubdir, file);
  const watcher = fs.watch(testsubdir, { recursive: true });

  let interval;

  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(filePath, 'world');
    }, 500);
  }));

  try {
    for await (const payload of watcher) {
      const { eventType, filename } = payload;

      assert.ok(eventType === 'change' || eventType === 'rename');

      if (filename === file) {
        clearInterval(interval);
        interval = null;
        break;
      }
    }
  } catch (error) {
    if (error.name !== 'AbortError') {
      assert.fail(error);
    }
  }

  process.on('exit', function() {
    assert.ok(interval === null, 'watcher Object was not closed');
  });
})().then(common.mustCall());
