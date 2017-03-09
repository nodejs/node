'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

{
  const file = path.join(common.tmpDir, 'write-end-test0.txt');
  const stream = fs.createWriteStream(file);
  stream.end();
  stream.on('close', common.mustCall(function() { }));
}

{
  const file = path.join(common.tmpDir, 'write-end-test1.txt');
  const stream = fs.createWriteStream(file);
  stream.end('a\n', 'utf8');
  stream.on('close', common.mustCall(function() {
    const content = fs.readFileSync(file, 'utf8');
    assert.strictEqual(content, 'a\n');
  }));
}
