'use strict';

// Refs: https://github.com/nodejs/node/issues/58634
// fs.cpSync() must accept Buffer paths together with a filter function when
// recursively copying directories. The filter receives Buffer paths, and the
// tree is copied.

const common = require('../common');
const assert = require('assert');
const { cpSync, mkdirSync, writeFileSync, readFileSync } = require('fs');
const { join } = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const src = join(tmpdir.path, 'a');
const dest = join(tmpdir.path, 'b');
mkdirSync(join(src, 'c'), { recursive: true });
writeFileSync(join(src, 'c', 'file.txt'), 'data');

cpSync(Buffer.from(src), Buffer.from(dest), {
  recursive: true,
  filter: common.mustCallAtLeast((srcArg, destArg) => {
    assert.ok(Buffer.isBuffer(srcArg));
    assert.ok(Buffer.isBuffer(destArg));
    return true;
  }, 1),
});

assert.strictEqual(readFileSync(join(dest, 'c', 'file.txt'), 'utf8'), 'data');
