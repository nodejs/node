'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

if (common.isFreeBSD) {
  common.skip('Test currently not working on FreeBSD');
  return;
}

const fn = '新建文夹件.txt';
const a = path.join(common.tmpDir, fn);

const watcher1 = fs.watch(
  common.tmpDir,
  {encoding: 'hex'},
  (event, filename) => {
    if (filename)
      assert.equal(filename, 'e696b0e5bbbae69687e5a4b9e4bbb62e747874');
    watcher1.close();
  }
);

const watcher2 = fs.watch(
  common.tmpDir,
  (event, filename) => {
    if (filename)
      assert.equal(filename, fn);
    watcher2.close();
  }
);

const watcher3 = fs.watch(
  common.tmpDir,
  {encoding: 'buffer'},
  (event, filename) => {
    if (filename) {
      assert(filename instanceof Buffer);
      assert.equal(filename.toString('utf8'), fn);
    }
    watcher3.close();
  }
);

const fd = fs.openSync(a, 'w+');
fs.closeSync(fd);

process.on('exit', () => {
  fs.unlink(a);
});
