'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

common.requireNoPackageJSONAbove();

const assert = require('assert');
const { spawnSync } = require('child_process');
const { cpSync, rmSync } = require('fs');
const fixtures = require('../common/fixtures.js');
const tmpdir = require('../common/tmpdir.js');

{
  const policyFilepath = fixtures.path('policy-manifest', 'invalid.json');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    './fhqwhgads.js',
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /ERR_MANIFEST_INVALID_SPECIFIER/);
  assert.match(stderr, /pattern needs to have a single trailing "\*"/);
}

{
  tmpdir.refresh();
  const policyFilepath = tmpdir.resolve('file with % in its name.json');
  cpSync(fixtures.path('policy-manifest', 'invalid.json'), policyFilepath);
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    './fhqwhgads.js',
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /ERR_MANIFEST_INVALID_SPECIFIER/);
  assert.match(stderr, /pattern needs to have a single trailing "\*"/);
  rmSync(policyFilepath);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-exit.json');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    '-e',
    'require("os").cpus()',
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /ERR_MANIFEST_DEPENDENCY_MISSING/);
  assert.match(stderr, /does not list module as a dependency specifier for conditions: require, node, node-addons/);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-exit.json');
  const mainModuleBypass = fixtures.path('policy-manifest', 'main-module-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    mainModuleBypass,
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /ERR_MANIFEST_DEPENDENCY_MISSING/);
  assert.match(stderr, /does not list os as a dependency specifier for conditions: require, node, node-addons/);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-resource-exit.json');
  const objectDefinePropertyBypass = fixtures.path('policy-manifest', 'object-define-property-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    objectDefinePropertyBypass,
  ]);

  assert.strictEqual(result.status, 0);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-exit.json');
  const mainModuleBypass = fixtures.path('policy-manifest', 'main-module-proto-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    mainModuleBypass,
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /ERR_MANIFEST_DEPENDENCY_MISSING/);
  assert.match(stderr, /does not list os as a dependency specifier for conditions: require, node, node-addons/);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-exit.json');
  const mainModuleBypass = fixtures.path('policy-manifest', 'module-constructor-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    mainModuleBypass,
  ]);
  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /TypeError/);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'manifest-impersonate.json');
  const createRequireBypass = fixtures.path('policy-manifest', 'createRequire-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    createRequireBypass,
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /TypeError/);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-exit.json');
  const mainModuleBypass = fixtures.path('policy-manifest', 'main-constructor-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    mainModuleBypass,
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /TypeError/);
}

{
  const policyFilepath = fixtures.path('policy-manifest', 'onerror-exit.json');
  const mainModuleBypass = fixtures.path('policy-manifest', 'main-constructor-extensions-bypass.js');
  const result = spawnSync(process.execPath, [
    '--experimental-policy',
    policyFilepath,
    mainModuleBypass,
  ]);

  assert.notStrictEqual(result.status, 0);
  const stderr = result.stderr.toString();
  assert.match(stderr, /TypeError/);
}
