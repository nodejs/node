'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  const s = fs.createWriteStream(path.join(tmpdir.path, 'rw'));

  s.close(common.mustCall());
  s.close(common.mustCall());
}

{
  const s = fs.createWriteStream(path.join(tmpdir.path, 'rw2'));

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
  const s = fs.createWriteStream(path.join(tmpdir.path, 'rw'), {
    autoClose: false
  });

  s.close(common.mustCall());
  s.close(common.mustCall());
}
