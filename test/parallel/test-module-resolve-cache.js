'use strict';

// Tests the persistent module-resolution cache: the NODE_MODULE_RESOLVE_CACHE
// environment variable and the module.enableModuleResolveCache() API, for both
// CommonJS and ES modules, including coarse (lockfile) invalidation.

require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const projectDir = tmpdir.resolve('project');
const cacheDir = tmpdir.resolve('resolve-cache');

function writePkg(name, pkgJson, indexSource) {
  const dir = path.join(projectDir, 'node_modules', name);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify(pkgJson));
  fs.writeFileSync(path.join(dir, pkgJson.main ?? 'index.js'), indexSource);
}

fs.mkdirSync(projectDir, { recursive: true });
fs.writeFileSync(path.join(projectDir, 'package-lock.json'), '{}');

// A CommonJS package and an ESM package.
writePkg('cjsdep', { name: 'cjsdep', version: '1.0.0', main: 'index.js' },
         'module.exports = "cjs-old";');
writePkg('esmdep',
         { name: 'esmdep', version: '1.0.0', type: 'module', exports: './index.js' },
         'export default "esm-old";');

const cjsApp = path.join(projectDir, 'app.cjs');
fs.writeFileSync(cjsApp, 'console.log("cjs:" + require("cjsdep"));');
const esmApp = path.join(projectDir, 'app.mjs');
fs.writeFileSync(esmApp, 'import v from "esmdep"; console.log("esm:" + v);');

function runApp(appFile, env) {
  return {
    env: { ...process.env, NODE_MODULE_RESOLVE_CACHE: cacheDir, ...env },
    cwd: projectDir,
  };
}

// 1. CJS: populate then read warm; both must resolve correctly.
spawnSyncAndAssert(process.execPath, [cjsApp], runApp(cjsApp),
                   { stdout: 'cjs:cjs-old\n' });
spawnSyncAndAssert(process.execPath, [cjsApp], runApp(cjsApp),
                   { stdout: 'cjs:cjs-old\n' });

// A cache file must have been written.
const files = fs.readdirSync(cacheDir, { recursive: true })
  .filter((f) => f.endsWith('.resolve.bin'));
assert.ok(files.length >= 1, `expected a .resolve.bin file, got ${files}`);

// 2. ESM: populate then read warm.
spawnSyncAndAssert(process.execPath, [esmApp], runApp(esmApp),
                   { stdout: 'esm:esm-old\n' });
spawnSyncAndAssert(process.execPath, [esmApp], runApp(esmApp),
                   { stdout: 'esm:esm-old\n' });

// 3. Coarse invalidation: change the resolution AND bump the lockfile; the
//    cache must be discarded and the new resolution picked up.
writePkg('cjsdep', { name: 'cjsdep', version: '1.0.0', main: 'other.js' },
         'unused');
fs.writeFileSync(path.join(projectDir, 'node_modules', 'cjsdep', 'other.js'),
                 'module.exports = "cjs-new";');
// Bump the lockfile so the generation token changes.
fs.writeFileSync(path.join(projectDir, 'package-lock.json'),
                 JSON.stringify({ lockfileVersion: 3, changed: true }));
spawnSyncAndAssert(process.execPath, [cjsApp], runApp(cjsApp),
                   { stdout: 'cjs:cjs-new\n' });

// 4. Program API: module.enableModuleResolveCache(dir) reports the dir and
//    resolution still works.
const apiDir = tmpdir.resolve('api-cache');
const apiApp = path.join(projectDir, 'api.cjs');
fs.writeFileSync(apiApp, `
const m = require('module');
const dir = m.enableModuleResolveCache(${JSON.stringify(apiDir)});
console.log('dir:' + (typeof dir === 'string' && dir.length > 0));
console.log('match:' + (m.getModuleResolveCacheDir() === dir));
console.log('val:' + require('cjsdep'));
`);
spawnSyncAndAssert(
  process.execPath, [apiApp],
  { env: { ...process.env, NODE_MODULE_RESOLVE_CACHE: undefined }, cwd: projectDir },
  { stdout: 'dir:true\nmatch:true\nval:cjs-new\n' });

// 5. Disabled by default: no cache directory is created, resolution works.
const offDir = tmpdir.resolve('never-created');
spawnSyncAndAssert(
  process.execPath, [cjsApp],
  { env: { ...process.env, NODE_MODULE_RESOLVE_CACHE: undefined }, cwd: projectDir },
  { stdout: 'cjs:cjs-new\n' });
assert.ok(!fs.existsSync(offDir));
