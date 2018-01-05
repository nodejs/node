'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();

{
  const s = fs.createWriteStream(path.join(common.tmpDir, 'rw'));

  s.close(common.mustCall());
  s.close(common.mustCall());
}

{
  const s = fs.createWriteStream(path.join(common.tmpDir, 'rw2'));

  let emits = 0;
  s.on('close', () => {
    emits++;
  });

  s.close(common.mustCall(() => {
    assert.strictEqual(emits, 1);
    s.close(common.mustCall(() => {
      assert.strictEqual(emits, 1);
    }));
    process.nextTick(() => {
      s.close(common.mustCall(() => {
        assert.strictEqual(emits, 1);
      }));
    });
  }));
}

{
  const s = fs.createWriteStream(path.join(common.tmpDir, 'rw'), {
    autoClose: false
  });

  s.close(common.mustCall());
  s.close(common.mustCall());
}
