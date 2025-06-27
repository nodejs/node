'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();

const code = common.isWindows ? 'ENOENT' : 'ENOTDIR';

{
  const filePath = tmpdir.resolve('rmdir-recursive.txt');
  fs.writeFileSync(filePath, '');
  assert.throws(() => fs.rmdirSync(filePath, { recursive: true }), { code });
}
{
  const filePath = tmpdir.resolve('rmdir-recursive.txt');
  fs.writeFileSync(filePath, '');
  fs.rmdir(filePath, { recursive: true }, common.mustCall((err) => {
    assert.strictEqual(err.code, code);
  }));
}
{
  const filePath = tmpdir.resolve('rmdir-recursive.txt');
  fs.writeFileSync(filePath, '');
  assert.rejects(() => fs.promises.rmdir(filePath, { recursive: true }),
                 { code }).then(common.mustCall());
}
