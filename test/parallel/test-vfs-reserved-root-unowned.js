// Flags: --experimental-vfs --expose-internals
'use strict';

// The module loader manufactures paths under the reserved VFS root that no
// layer owns: resolving a mount point as a directory first probes the sibling
// names `<mount>.js`, `<mount>.json`, ..., and a package.json walk-up passes
// the parents of the mount point. Such paths cannot name anything real, but
// they must still be answered by the VFS instead of being handed to the native
// loader: on Windows the reserved root sits under `\\.\nul`, and
// `\\.\nul\<anything>` opens the NUL device, which stats as a character device
// and reads as empty. The native loader would then "find" a file at
// `<mount>.js` and reject the empty package.json above it as invalid JSON.

require('../common');
const assert = require('assert');
const path = require('path');
const { pathToFileURL } = require('url');
const vfs = require('node:vfs');
const { loaderMethods } = require('internal/modules/helpers');
const { getNormalizedVfsRoot } = require('internal/vfs/router');

const layer = vfs.create();
layer.writeFileSync('/index.js', 'module.exports = "ran";');
const mountPoint = layer.mount();

const root = getNormalizedVfsRoot();
const unowned = [
  `${mountPoint}.js`,
  `${mountPoint}.json`,
  `${mountPoint}.node`,
  path.join(root, 'package.json'),
  path.join(root, 'nope', 'index.js'),
];

for (const p of unowned) {
  assert.ok(loaderMethods.internalModuleStat(p) < 0, p);
  assert.throws(() => loaderMethods.readFileSync(p), { code: 'ENOENT' }, p);
  assert.throws(() => loaderMethods.realpathSync(p), { code: 'ENOENT' }, p);
  assert.strictEqual(loaderMethods.getNearestParentPackageJSON(p), undefined, p);
  assert.strictEqual(loaderMethods.readPackageJSON(p, false), undefined, p);
  assert.strictEqual(loaderMethods.getPackageType(pathToFileURL(p).href), undefined, p);
  // The "not found" marker is the last candidate examined, like the native
  // binding returns.
  assert.strictEqual(
    loaderMethods.getPackageScopeConfig(pathToFileURL(p).href),
    path.join(path.dirname(p), 'package.json'), p);
  // Upward walks (node_modules lookups) stop at the reserved root rather than
  // continuing into the real file system.
  assert.strictEqual(loaderMethods.getResolutionRoot(p), root, p);
}

// Paths outside the reserved root are still left to the native loader.
assert.strictEqual(loaderMethods.getResolutionRoot(__filename), undefined);

// The mount point itself resolves as a directory to its index through the
// layer, which is the sequence that produced the sibling probes above.
assert.strictEqual(require(mountPoint), 'ran');

layer.unmount();
