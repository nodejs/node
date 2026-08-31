'use strict';

// Refs: https://github.com/nodejs/node/issues/58634
// fs.promises.cp() must accept Buffer paths for src and dest, matching
// fs.cpSync(), and copy the directory tree.

const common = require('../common');
const assert = require('assert');
const { mkdirSync, writeFileSync, readFileSync, promises } = require('fs');
const { join } = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const src = join(tmpdir.path, 'a');
const dest = join(tmpdir.path, 'b');
mkdirSync(join(src, 'sub'), { recursive: true });
writeFileSync(join(src, 'file.txt'), 'hello');
writeFileSync(join(src, 'sub', 'nested.txt'), 'world');

promises.cp(Buffer.from(src), Buffer.from(dest), { recursive: true })
  .then(common.mustCall(() => {
    assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'hello');
    assert.strictEqual(
      readFileSync(join(dest, 'sub', 'nested.txt'), 'utf8'), 'world');
  }));
