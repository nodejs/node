'use strict';

const common = require('../common');
if (!process.config.variables.node_use_amaro) {
  common.skip('Requires Amaro');
}

const assert = require('assert');
const { configureTypeScript, stripTypeScriptTypes } = require('node:module');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

test('configureTypeScript export and basic getters', () => {
  assert.strictEqual(typeof configureTypeScript, 'function');

  const current = configureTypeScript();
  assert.strictEqual(typeof current, 'object');
  assert.strictEqual(typeof current.mode, 'string');
  assert.strictEqual(typeof current.nodeModules, 'boolean');

  const original = configureTypeScript({ mode: 'transform' });
  assert.deepStrictEqual(original, current);

  const updated = configureTypeScript({ mode: 'strip' });
  assert.strictEqual(updated.mode, 'transform');

  configureTypeScript(original);
});

test('configureTypeScript input validation', () => {
  assert.throws(() => configureTypeScript('invalid'), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => configureTypeScript({ mode: 'invalid' }), { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => configureTypeScript({ nodeModules: 'invalid' }), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => configureTypeScript({ sourceMaps: 'invalid' }), { code: 'ERR_INVALID_ARG_TYPE' });

  // Valid sourceMaps value round-trips correctly (covers sourceMaps config path)
  const prev = configureTypeScript({ sourceMaps: true });
  assert.strictEqual(configureTypeScript().sourceMaps, true);
  configureTypeScript(prev);
});

test('stripTypeScriptTypes transform mode', () => {
  const source = 'enum MyEnum { A, B }';
  assert.throws(() => stripTypeScriptTypes(source, { mode: 'strip' }), {
    code: 'ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX'
  });

  const transformed = stripTypeScriptTypes(source, { mode: 'transform' });
  assert.match(transformed, /MyEnum/);
});

test('process.features.typescript reflects API changes', () => {
  const original = configureTypeScript();

  configureTypeScript({ mode: 'transform' });
  assert.strictEqual(process.features.typescript, 'transform');

  configureTypeScript({ mode: 'strip' });
  assert.strictEqual(process.features.typescript, 'strip');

  configureTypeScript(original);
});

// Covers getCachedCodeType transform branch in-process.
// Also exercises kTransformedTypeScript path in compile_cache.cc.
test('in-process CJS loading with transform mode', () => {
  tmpdir.refresh();

  const tsFile = path.join(tmpdir.path, 'fixture.cts');
  fs.writeFileSync(tsFile, [
    'enum Color { Red = 1 }',
    'module.exports = { Color };',
  ].join('\n'));

  const prev = configureTypeScript({ mode: 'transform' });
  try {
    const { Color } = require(tsFile);
    assert.strictEqual(Color.Red, 1);
  } finally {
    configureTypeScript(prev);
  }
});

// Covers getCachedCodeType with sourceMaps: true (kTransformedTypeScriptWithSourceMaps).
test('in-process CJS loading with transform + sourceMaps', () => {
  tmpdir.refresh();

  const tsFile = path.join(tmpdir.path, 'fixture2.cts');
  fs.writeFileSync(tsFile, [
    'enum Direction { Up = 0 }',
    'module.exports = { Direction };',
  ].join('\n'));

  const prev = configureTypeScript({ mode: 'transform', sourceMaps: true });
  try {
    const { Direction } = require(tsFile);
    assert.strictEqual(Direction.Up, 0);
  } finally {
    configureTypeScript(prev);
  }
});

// Covers the evalTypeScriptModuleEntryPoint branch in execution.js.
// --experimental-detect-module + ESM syntax triggers it when TypeScript is enabled.
test('eval with detect-module and strip-types uses TypeScript module entry point', () => {
  const child = spawnSync(process.execPath, [
    '--experimental-detect-module',
    '--experimental-strip-types',
    '--eval',
    'import "node:path"; const x: number = 1; console.log("ok");',
  ], { encoding: 'utf8' });

  assert.strictEqual(child.status, 0, child.stderr);
  assert.match(child.stdout, /ok/);
});

// Test nodeModules option and dynamic loading in a spawned process
test('nodeModules option and dynamic loading behavior', () => {
  tmpdir.refresh();

  const nodeModulesDir = path.join(tmpdir.path, 'node_modules');
  fs.mkdirSync(nodeModulesDir, { recursive: true });
  const pkgDir = path.join(nodeModulesDir, 'my-pkg');
  fs.mkdirSync(pkgDir, { recursive: true });

  fs.writeFileSync(path.join(pkgDir, 'index.ts'), 'export const x: number = 42;');
  fs.writeFileSync(
    path.join(pkgDir, 'package.json'),
    JSON.stringify({ type: 'module', main: './index.ts' })
  );

  const scriptFile = path.join(tmpdir.path, 'test.mjs');
  const script = [
    'import assert from "assert";',
    'import { configureTypeScript } from "node:module";',
    'configureTypeScript({ mode: "strip", nodeModules: false });',
    'try {',
    '  await import("my-pkg");',
    '  assert.fail("Should have thrown");',
    '} catch (err) {',
    '  assert.strictEqual(err.code, "ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING");',
    '}',
    'configureTypeScript({ nodeModules: true });',
    'const { x } = await import("my-pkg");',
    'assert.strictEqual(x, 42);',
    'console.log("OK");',
  ].join('\n');
  fs.writeFileSync(scriptFile, script);

  const child = spawnSync(process.execPath, [scriptFile], { encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
  assert.match(child.stdout, /OK/);
});

// Test compile cache key selection based on mode and sourceMaps
test('compile cache key selection and transform mode in runtime CJS loading', () => {
  tmpdir.refresh();

  const script = [
    'const assert = require("assert");',
    'const { configureTypeScript } = require("node:module");',
    'configureTypeScript({ mode: "transform", sourceMaps: true });',
    'const other = require("./other.cts");',
    'assert.strictEqual(other.MyEnum.Val, 100);',
    'console.log("OK-CJS");',
  ].join('\n');
  const scriptFile = path.join(tmpdir.path, 'script.cts');
  fs.writeFileSync(scriptFile, script);

  const otherFile = path.join(tmpdir.path, 'other.cts');
  fs.writeFileSync(otherFile, [
    'enum MyEnum { Val = 100 }',
    'module.exports = { MyEnum };',
  ].join('\n'));

  const cacheDir = path.join(tmpdir.path, 'cache');

  const child = spawnSync(process.execPath, [scriptFile], {
    env: { ...process.env, NODE_DEBUG_NATIVE: 'COMPILE_CACHE', NODE_COMPILE_CACHE: cacheDir },
    encoding: 'utf8'
  });

  assert.strictEqual(child.status, 0, child.stderr);
  assert.match(child.stdout, /OK-CJS/);
  assert.match(child.stderr, /writing cache for TransformedTypeScriptWithSourceMaps/);
});
