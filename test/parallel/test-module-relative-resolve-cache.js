'use strict';

// Checks that Module._load() only creates a relative resolve cache entry for
// the parent directory when there is a resolved filename to store in it.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Module = require('module');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

let uniqueId = 0;

function createParent() {
  const dir = tmpdir.resolve(`dir-${uniqueId++}`);
  fs.mkdirSync(dir);
  const parent = new Module(path.join(dir, 'parent.js'));
  parent.filename = parent.id;
  parent.paths = Module._nodeModulePaths(dir);
  let pathReads = 0;
  Object.defineProperty(parent, 'path', {
    __proto__: null,
    get() {
      pathReads++;
      return dir;
    },
  });
  return { dir, parent, getPathReads: () => pathReads };
}

// Builtins return before the relative resolve cache is populated, so the
// parent directory should only be looked up, not also used to create a bucket.
for (const request of ['node:path', 'path']) {
  const { parent, getPathReads } = createParent();
  assert.strictEqual(Module._load(request, parent, false), path);
  assert.strictEqual(getPathReads(), 1);
}

// Same for resolutions that throw.
{
  const { parent, getPathReads } = createParent();
  assert.throws(() => Module._load('./missing', parent, false),
                { code: 'MODULE_NOT_FOUND' });
  assert.strictEqual(getPathReads(), 1);
}

// A successful load populates the cache, and the second load hits it.
{
  const { dir, parent, getPathReads } = createParent();
  fs.writeFileSync(path.join(dir, 'child.js'), 'module.exports = {};');
  const first = Module._load('./child', parent, false);
  assert.strictEqual(getPathReads(), 1);
  assert.strictEqual(Module._load('./child', parent, false), first);
  assert.strictEqual(getPathReads(), 2);
}
