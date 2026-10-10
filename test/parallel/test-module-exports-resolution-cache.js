'use strict';

// Checks the cache for packages resolved through package.json "exports" in
// Module._findPath(): it must not serve other conditions, and replacing
// Module._pathCache must still force a fresh resolution.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Module = require('module');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

function writePackage(dir, files) {
  fs.mkdirSync(dir, { recursive: true });
  for (const [name, contents] of Object.entries(files)) {
    fs.writeFileSync(path.join(dir, name), contents);
  }
}

writePackage(tmpdir.resolve('node_modules', 'pkg'), {
  'package.json': JSON.stringify({
    name: 'pkg',
    exports: { custom: './custom.js', default: './default.js' },
  }),
  'custom.js': '',
  'default.js': '',
});

// Other conditions are resolved, not served the cached default result.
{
  const paths = Module._nodeModulePaths(tmpdir.path);
  const defaultTarget = tmpdir.resolve('node_modules', 'pkg', 'default.js');
  assert.strictEqual(Module._findPath('pkg', paths), defaultTarget);
  assert.strictEqual(Module._findPath('pkg', paths), defaultTarget);
  assert.strictEqual(
    Module._findPath('pkg', paths, false, new Set(['custom', 'require', 'node'])),
    tmpdir.resolve('node_modules', 'pkg', 'custom.js'),
  );
}

// A package installed closer to the parent is found once Module._pathCache
// has been replaced.
{
  const dir = tmpdir.resolve('nested');
  fs.mkdirSync(dir);
  const paths = Module._nodeModulePaths(dir);
  assert.strictEqual(Module._findPath('pkg', paths),
                     tmpdir.resolve('node_modules', 'pkg', 'default.js'));

  writePackage(path.join(dir, 'node_modules', 'pkg'), {
    'package.json': JSON.stringify({ name: 'pkg', exports: './nearer.js' }),
    'nearer.js': '',
  });
  Module._pathCache = { __proto__: null };
  assert.strictEqual(Module._findPath('pkg', paths),
                     path.join(dir, 'node_modules', 'pkg', 'nearer.js'));
}
