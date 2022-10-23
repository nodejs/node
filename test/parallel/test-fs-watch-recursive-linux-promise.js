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

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

(async () => {
  {
    // Add a file to already watching folder

    const testsubdir = await fs.mkdtemp(testDir + path.sep);
    const file = `${randomUUID()}.txt`;
    const filePath = path.join(testsubdir, file);
    const watcher = fs.watch(testsubdir, { recursive: true });

    setTimeout(async () => {
      await fs.writeFile(filePath, 'world');
    }, 100);

    for await (const payload of watcher) {
      const { eventType, filename } = payload;

      assert.ok(eventType === 'change' || eventType === 'rename');

      if (filename === file) {
        break;
      }
    }
  }
})().then(common.mustCall());
