'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { buffer } = require('stream/consumers');
const { open } = fs.promises;
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

async function validateRepeatedStreamsRemoveListeners(filePath) {
  const fileHandle = await open(filePath, 'r');
  try {
    const initialListeners = fileHandle.listenerCount('close');

    for (let i = 0; i < 100; i++) {
      const stream = fileHandle.createReadStream({
        start: i,
        end: i,
        autoClose: false,
      });
      const data = await buffer(stream);
      assert.strictEqual(data.length, 1);
    }

    assert.strictEqual(fileHandle.listenerCount('close'), initialListeners);
  } finally {
    await fileHandle.close();
  }
}

(async function() {
  const filePath = path.resolve(tmpdir.path, 'readstream-listener-leak');
  fs.writeFileSync(filePath, 'x'.repeat(100));

  await validateRepeatedStreamsRemoveListeners(filePath);
})().then(common.mustCall());
