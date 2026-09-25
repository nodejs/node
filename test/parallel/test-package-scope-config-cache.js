// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const tmpdir = require('../common/tmpdir');
const { getPackageScopeConfig } = require('internal/modules/package_json_reader');

tmpdir.refresh();
const packageConfig = {
  name: 'scope-cache',
  type: 'module',
  exports: { '.': './index.js' },
  imports: { '#dep': './dep.js' },
};
fs.writeFileSync(tmpdir.resolve('package.json'), JSON.stringify(packageConfig));
fs.mkdirSync(tmpdir.resolve('nested'));
fs.writeFileSync(tmpdir.resolve('nested', 'package.json'), JSON.stringify({
  type: 'commonjs',
  exports: ['./other.js'],
}));

const first = getPackageScopeConfig(pathToFileURL(tmpdir.resolve('index.js')));
const second = getPackageScopeConfig(pathToFileURL(tmpdir.resolve('dep.js')).href);
assert.strictEqual(first.pjsonPath,
                   path.toNamespacedPath(tmpdir.resolve('package.json')));
assert.strictEqual(first.type, 'module');
assert.deepStrictEqual(first.exports, packageConfig.exports);
assert.deepStrictEqual(first.imports, packageConfig.imports);
assert.strictEqual(second.exports, first.exports);
assert.strictEqual(second.imports, first.imports);

// Each caller still receives its own configuration wrapper.
first.type = 'commonjs';
assert.strictEqual(second.type, 'module');
assert.strictEqual(getPackageScopeConfig(
  pathToFileURL(tmpdir.resolve('third.js'))).type, 'module');

const nested = getPackageScopeConfig(pathToFileURL(tmpdir.resolve('nested', 'index.js')));
assert.strictEqual(nested.pjsonPath,
                   path.toNamespacedPath(tmpdir.resolve('nested', 'package.json')));
assert.strictEqual(nested.type, 'commonjs');
assert.deepStrictEqual(nested.exports, ['./other.js']);
assert.strictEqual(nested.imports, undefined);

// Package scope traversal must still stop at node_modules.
fs.mkdirSync(tmpdir.resolve('node_modules', 'bare'), { recursive: true });
const missing = getPackageScopeConfig(
  pathToFileURL(tmpdir.resolve('node_modules', 'bare', 'index.js')));
assert.strictEqual(missing.exists, false);
assert.strictEqual(missing.type, 'none');
assert.strictEqual(missing.pjsonPath,
                   path.join(tmpdir.path, 'node_modules', 'package.json'));
