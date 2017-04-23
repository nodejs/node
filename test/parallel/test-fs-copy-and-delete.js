'use strict';
const common = require('../common');
//This test tests fs.copy,  fs.copySync,  fs.delete and fs.deleteSync
const fs = require('fs');
const assert = require('assert');
common.refreshTmpDir();
const testDir = common.tmpDir + '/copyanddeletetest';

if (!fs.existsSync(testDir)) {
  fs.mkdirSync(testDir);
}

function setUpDir(path) {

  if (!fs.existsSync(path)) {
    fs.mkdirSync(path);
  }
  fs.mkdirSync(path + '/test1');
  fs.mkdirSync(path + '/test2');
  fs.mkdirSync(path + '/test3');
  fs.writeFileSync(path + '/file1', 'Contents of file\r\n1');
  fs.writeFileSync(path + '/file2', 'Contents of file 2');
  fs.writeFileSync(path + '/test1/file3', 'Contents of file\r\n3');
  fs.writeFileSync(path + '/test3/file4', 'Contents of file 4');
  fs.writeFileSync(path + '/test3/file5', 'Contents of file 5');

}

setUpDir(testDir + '/test1');

setTimeout(() => {

  fs.copySync(testDir + '/test1', testDir + '/test1-copied');
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test1`), true);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test2`), true);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test3`), true);
  assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/file1`)
    .toString(), 'Contents of file\r\n1');
  assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/file2`)
    .toString(), 'Contents of file 2');
  assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/test1/file3`)
    .toString(), 'Contents of file\r\n3');
  assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/test3/file4`)
    .toString(), 'Contents of file 4');
  assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/test3/file5`)
    .toString(), 'Contents of file 5');

  function checkProps(p1, p2) {

    assert.strictEqual(fs.statSync(p1).mode, fs.statSync(p2).mode);
    assert.strictEqual(fs.statSync(p1).uid, fs.statSync(p2).uid);
    assert.strictEqual(fs.statSync(p1).gid, fs.statSync(p2).gid);
    assert.strictEqual(fs.statSync(p1).size, fs.statSync(p2).size);
    if (process.platform !== 'aix' &&
    Math.abs(fs.statSync(p1).mtime.getTime() -
    fs.statSync(p2).mtime.getTime()) > 1000) {

      assert.fail(Math.abs(fs.statSync(p1).mtime.getTime() -
                          fs.statSync(p2).mtime.getTime()), 1000,
                  'mtimes are not correct.',
                  '<');

    }

  }

  checkProps(`${testDir}/test1/test1`, `${testDir}/test1-copied/test1`);
  checkProps(`${testDir}/test1/test2`, `${testDir}/test1-copied/test2`);
  checkProps(`${testDir}/test1/test3`, `${testDir}/test1-copied/test3`);
  checkProps(`${testDir}/test1/file1`, `${testDir}/test1-copied/file1`);
  checkProps(`${testDir}/test1/file2`, `${testDir}/test1-copied/file2`);
  checkProps(`${testDir}/test1/test1/file3`,
             `${testDir}/test1-copied/test1/file3`);
  checkProps(`${testDir}/test1/test3/file4`,
             `${testDir}/test1-copied/test3/file4`);
  checkProps(`${testDir}/test1/test3/file5`,
             `${testDir}/test1-copied/test3/file5`);

  fs.deleteSync(testDir + '/test1-copied');

  assert.strictEqual(common.fileExists(`${testDir}/test1-copied`), false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test1`), false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test2`), false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test3`), false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/file1`), false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/file2`), false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test1/file3`),
                     false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test3/file4`),
                     false);
  assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test3/file5`),
                     false);

  fs.copy(testDir + '/test1', testDir + '/test1-copied', (err) => {

    if (err) {

      assert.ifError(err);

    } else {

      assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test1`),
                         true);
      assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test2`),
                         true);
      assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test3`),
                         true);
      assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/file1`)
        .toString(), 'Contents of file\r\n1');
      assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/file2`)
        .toString(), 'Contents of file 2');
      assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/test1/file3`)
        .toString(), 'Contents of file\r\n3');
      assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/test3/file4`)
        .toString(), 'Contents of file 4');
      assert.strictEqual(fs.readFileSync(`${testDir}/test1-copied/test3/file5`)
        .toString(), 'Contents of file 5');

      checkProps(`${testDir}/test1/test1`, `${testDir}/test1-copied/test1`);
      checkProps(`${testDir}/test1/test2`, `${testDir}/test1-copied/test2`);
      checkProps(`${testDir}/test1/test3`, `${testDir}/test1-copied/test3`);
      checkProps(`${testDir}/test1/file1`, `${testDir}/test1-copied/file1`);
      checkProps(`${testDir}/test1/file2`, `${testDir}/test1-copied/file2`);
      checkProps(`${testDir}/test1/test1/file3`,
                 `${testDir}/test1-copied/test1/file3`);
      checkProps(`${testDir}/test1/test3/file4`,
                 `${testDir}/test1-copied/test3/file4`);
      checkProps(`${testDir}/test1/test3/file5`,
                 `${testDir}/test1-copied/test3/file5`);

      fs.delete(testDir + '/test1-copied', (err) => {

        if (err) {

          assert.ifError(err);

        } else {

          assert.strictEqual(common.fileExists(`${testDir}/test1-copied`),
                             false);
          assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test1`)
                             , false);
          assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test2`)
                             , false);
          assert.strictEqual(common.fileExists(`${testDir}/test1-copied/test3`)
                             , false);
          assert.strictEqual(common.fileExists(`${testDir}/test1-copied/file1`)
                             , false);
          assert.strictEqual(common.fileExists(`${testDir}/test1-copied/file2`)
                             , false);
          assert.strictEqual(common
            .fileExists(`${testDir}/test1-copied/test1/file3`), false);
          assert.strictEqual(common
            .fileExists(`${testDir}/test1-copied/test3/file4`), false);
          assert.strictEqual(common
            .fileExists(`${testDir}/test1-copied/test3/file5`), false);

          fs.deleteSync(`${testDir}/test1`);

        }

      });

    }

  });

}, 1500);
