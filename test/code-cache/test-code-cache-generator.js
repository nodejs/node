'use strict';

// This test verifies the code cache generator can generate a C++
// file that contains the code cache. This can be removed once we
// actually build that C++ file into our binary.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { spawnSync } = require('child_process');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const readline = require('readline');

console.log('Looking for mkcodecache executable');
let buildDir;
const stat = fs.statSync(process.execPath);
if (stat.isSymbolicLink()) {
  console.log('Binary is a symbolic link');
  buildDir = path.dirname(fs.readlinkSync(process.execPath));
} else {
  buildDir = path.dirname(process.execPath);
}

const ext = common.isWindows ? '.exe' : '';
const generator = path.join(buildDir, `mkcodecache${ext}`);
if (!fs.existsSync(generator)) {
  common.skip('Could not find mkcodecache');
}

console.log(`mkcodecache is ${generator}`);

tmpdir.refresh();
const dest = path.join(tmpdir.path, 'cache.cc');

// Run mkcodecache
const child = spawnSync(generator, [dest]);
assert.ifError(child.error);
if (child.status !== 0) {
  console.log(child.stderr.toString());
  assert.strictEqual(child.status, 0);
}

// Verifies that:
// - NativeModuleEnv::InitializeCodeCache()
// are defined in the generated code.
// See src/node_native_module_env.h for explanations.

const rl = readline.createInterface({
  input: fs.createReadStream(dest),
  crlfDelay: Infinity
});

let hasCacheDef = false;

rl.on('line', common.mustCallAtLeast((line) => {
  if (line.includes('InitializeCodeCache(')) {
    hasCacheDef = true;
  }
}, 2));

rl.on('close', common.mustCall(() => {
  assert.ok(hasCacheDef);
}));
