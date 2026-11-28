'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const tmpdir = require('../common/tmpdir');
const {
  getNearestParentPackageJSON,
  getRootPackageJSON,
  getPackageScopeConfig,
  getPackageType,
} = require('internal/modules/package_json_reader');

tmpdir.refresh();

const lookups = {
  nearestParent: (file) => getNearestParentPackageJSON(file).data.type,
  root: (file) => getRootPackageJSON(file).data.type,
  scope: (file) => getPackageScopeConfig(pathToFileURL(file)).type,
  type: (file) => getPackageType(pathToFileURL(file)),
};

for (const [name, lookup] of Object.entries(lookups)) {
  for (const failure of ['invalid', 'directory']) {
    const outer = tmpdir.resolve(`${name}-${failure}`);
    const parent = name === 'root' ?
      path.join(outer, 'node_modules', 'outer') : outer;
    const inner = name === 'root' ?
      path.join(parent, 'node_modules', 'inner') : path.join(parent, 'inner');
    const manifest = path.join(inner, 'package.json');
    const parentManifest = path.join(parent, 'package.json');
    const file = path.join(inner, 'lib', 'index.js');
    fs.mkdirSync(path.dirname(file), { recursive: true });

    // Continuing past the first error would replace it with this parent's error.
    fs.writeFileSync(parentManifest, '{');
    if (failure === 'invalid') {
      fs.writeFileSync(manifest, '{');
    } else {
      fs.mkdirSync(manifest);

      // Some platforms allow reading a directory.
      let directoryReadError;
      try {
        fs.readFileSync(manifest);
      } catch (err) {
        directoryReadError = err;
      }
      if (!directoryReadError) {
        continue;
      }
      assert.strictEqual(directoryReadError.code, 'EISDIR');
    }

    const expectedError = {
      code: failure === 'invalid' ?
        'ERR_INVALID_PACKAGE_CONFIG' : 'ERR_ACCESS_DENIED',
      message: new RegExp(RegExp.escape(path.toNamespacedPath(manifest))),
    };
    assert.throws(() => lookup(file), expectedError);

    // A failed lookup must not cache the manifest as missing.
    assert.throws(() => lookup(file), expectedError);

    if (failure === 'directory') {
      fs.rmdirSync(manifest);
    }
    const config = JSON.stringify({ type: 'module', private: true });
    fs.writeFileSync(manifest, config);
    fs.writeFileSync(parentManifest, config);
    assert.strictEqual(lookup(file), 'module');
  }
}
