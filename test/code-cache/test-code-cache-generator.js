'use strict';

// This test verifies that the binary is compiled with code cache and the
// cache is used when built in modules are compiled.

const common = require('../common');

const tmpdir = require('../common/tmpdir');
const { spawnSync } = require('child_process');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const readline = require('readline');

const generator = path.join(
  __dirname, '..', '..', 'tools', 'generate_code_cache.js'
);
tmpdir.refresh();
const dest = path.join(tmpdir.path, 'cache.cc');

// Run tools/generate_code_cache.js
const child = spawnSync(
  process.execPath,
  ['--expose-internals', generator, dest]
);
assert.ifError(child.error);
if (child.status !== 0) {
  console.log(child.stderr.toString());
  assert.strictEqual(child.status, 0);
}

// Verifies that:
// - node::LoadCodeCache()
// are defined in the generated code.
// See src/node_native_module.h for explanations.

const rl = readline.createInterface({
  input: fs.createReadStream(dest),
  crlfDelay: Infinity
});

let hasCacheDef = false;

rl.on('line', common.mustCallAtLeast((line) => {
  if (line.includes('LoadCodeCache(')) {
    hasCacheDef = true;
  }
}, 2));

rl.on('close', common.mustCall(() => {
  assert.ok(hasCacheDef);
}));
