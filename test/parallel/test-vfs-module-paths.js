// Flags: --experimental-vfs
'use strict';

// The `node_modules` lookup for modules inside a mounted VFS stops at the
// mount point: no candidate directories above the mount root are generated,
// while the legacy CommonJS global folders (NODE_PATH etc.) still apply.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { execFileSync } = require('child_process');
const Module = require('module');
const vfs = require('node:vfs');
const tmpdir = require('../common/tmpdir');

// Test: module.paths for a CJS module inside a mounted VFS ends at the
// mount root.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/foo/bar', { recursive: true });
  myVfs.writeFileSync('/foo/bar/main.cjs', 'module.exports = module.paths;');
  const mountPoint = myVfs.mount();

  const paths = require(path.join(mountPoint, 'foo/bar/main.cjs'));
  assert.deepStrictEqual(paths, [
    path.join(mountPoint, 'foo/bar/node_modules'),
    path.join(mountPoint, 'foo/node_modules'),
    path.join(mountPoint, 'node_modules'),
  ]);

  myVfs.unmount();
}

// Test: Module._nodeModulePaths for real paths still walks to the file
// system root.
{
  const paths = Module._nodeModulePaths(__dirname);
  const root = path.parse(__dirname).root;
  assert.strictEqual(paths[paths.length - 1],
                     path.join(root, 'node_modules'));
}

// Test: bare specifiers resolve through node_modules directories inside
// the VFS, for both require() and import.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/dep', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/dep/package.json',
                      '{"name":"dep","main":"index.js"}');
  myVfs.writeFileSync('/app/node_modules/dep/index.js',
                      'module.exports = "dep-from-vfs";');
  myVfs.writeFileSync('/app/main.cjs', 'module.exports = require("dep");');
  myVfs.writeFileSync('/app/main.mjs',
                      'import dep from "dep"; export default dep;');
  const mountPoint = myVfs.mount();

  assert.strictEqual(require(path.join(mountPoint, 'app/main.cjs')),
                     'dep-from-vfs');

  import(pathToFileURL(path.join(mountPoint, 'app/main.mjs')).href)
    .then(common.mustCall((ns) => {
      assert.strictEqual(ns.default, 'dep-from-vfs');
      myVfs.unmount();
    }));
}

// Test: a bare specifier that exists nowhere fails for both loaders.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/missing.cjs', 'require("missing-pkg-xyz");');
  myVfs.writeFileSync('/missing.mjs', 'import "missing-pkg-xyz";');
  const mountPoint = myVfs.mount();

  assert.throws(() => require(path.join(mountPoint, 'missing.cjs')),
                { code: 'MODULE_NOT_FOUND' });

  assert.rejects(
    import(pathToFileURL(path.join(mountPoint, 'missing.mjs')).href),
    { code: 'ERR_MODULE_NOT_FOUND' },
  ).then(() => myVfs.unmount()).then(common.mustCall());
}

// Test: the legacy CommonJS global folders still apply to require() from
// inside a VFS: a NODE_PATH directory provides the fallback.
{
  tmpdir.refresh();
  const globalDir = tmpdir.resolve('gpath');
  fs.mkdirSync(path.join(globalDir, 'gdep'), { recursive: true });
  fs.writeFileSync(path.join(globalDir, 'gdep', 'index.js'),
                   'module.exports = "from-node-path";');

  const out = execFileSync(process.execPath, ['--experimental-vfs', '-e', `
    const vfs = require('node:vfs');
    const path = require('path');
    const myVfs = vfs.create();
    myVfs.writeFileSync('/main.cjs', 'console.log(require("gdep"));');
    const mountPoint = myVfs.mount();
    require(path.join(mountPoint, 'main.cjs'));
  `], {
    encoding: 'utf8',
    env: { ...process.env, NODE_PATH: globalDir, NODE_NO_WARNINGS: '1' },
  });
  assert.strictEqual(out.trim(), 'from-node-path');
}
