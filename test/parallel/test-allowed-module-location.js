'use strict';
// Flags: --expose-internals
require('../common');
const { getOptionValue } = require('internal/options');
const assert = require('assert');
const path = require('path');
const cp = require('child_process');

if (getOptionValue('--allowed-module-location').length === 0) {
  const args = [
    '--expose-internals',
    '--allowed-module-location', __dirname,
    '--allowed-module-location', path.join(__dirname, '..', 'common'),
    __filename
  ];
  const result = cp.spawnSync(process.argv0,
                              args,
                              { stdio: 'inherit', shell: false });
  assert.strictEqual(result.status, 0);
  process.exit(0);
}

const requireModule = path.join('..', 'fixtures', 'allowed-modules');
assert.throws(
  () => { require(requireModule); },
  {
    code: 'ERR_MODULE_BLOCKED',
    name: 'Error',
    message: `Loading of module ${requireModule} was blocked: ` +
             'module path is outside allowed locations'
  });

const requireFile = path.join(requireModule, 'index.js');
assert.throws(
  () => { require(requireFile); },
  {
    code: 'ERR_MODULE_BLOCKED',
    name: 'Error',
    message: `Loading of module ${requireFile} was blocked: ` +
             'module path is outside allowed locations'
  });
