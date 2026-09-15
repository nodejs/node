// Flags: --experimental-vfs --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const { pathToFileURL } = require('url');
const vfs = require('node:vfs');
const { getPackageScopeConfig } = require('internal/modules/package_json_reader');

const volume = vfs.create();
volume.mkdirSync('/pkg');
const mountPoint = volume.mount();
const packagePath = path.join(mountPoint, 'pkg', 'package.json');
const url = pathToFileURL(path.join(mountPoint, 'pkg', 'index.js'));
const config = {
  name: 'before',
  main: './index.js',
  type: 'module',
  imports: { '#dep': './dep.js' },
  exports: { '.': './index.js' },
};

function readConfig() {
  volume.writeFileSync(packagePath, JSON.stringify(config));
  const result = getPackageScopeConfig(url);
  for (const key of Object.keys(config)) {
    assert.deepStrictEqual(result[key], config[key]);
  }
  assert.strictEqual(result.exists, true);
  assert.strictEqual(result.pjsonPath, path.join(mountPoint, 'pkg', 'package.json'));
  return result;
}

try {
  const first = readConfig();
  assert.strictEqual(getPackageScopeConfig(url).exports, first.exports);

  // Invalidate on each serialized field independently, including fields that
  // are not used to resolve package imports.
  for (const [key, value] of Object.entries({
    name: 'after',
    main: './other.js',
    type: 'commonjs',
    imports: { '#dep': './other.js' },
    exports: ['./other.js'],
  })) {
    config[key] = value;
    readConfig();
  }

  const beforePurge = getPackageScopeConfig(url);
  volume.unmount();
  assert.strictEqual(volume.mount(), mountPoint);
  const afterPurge = getPackageScopeConfig(url);
  assert.deepStrictEqual(afterPurge, beforePurge);
  assert.notStrictEqual(afterPurge.exports, beforePurge.exports);

  config.exports = './direct.js';
  readConfig();
  delete config.imports;
  assert.strictEqual(readConfig().imports, undefined);
  delete config.exports;
  assert.strictEqual(readConfig().exports, undefined);

  // A warm cache must not hide parse errors or a removed package.json.
  volume.writeFileSync(packagePath, '{');
  assert.throws(() => getPackageScopeConfig(url), { code: 'ERR_INVALID_PACKAGE_CONFIG' });
  volume.unlinkSync(packagePath);
  assert.strictEqual(getPackageScopeConfig(url).exists, false);
} finally {
  volume.unmount();
}
