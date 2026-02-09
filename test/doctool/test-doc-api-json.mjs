import * as common from '../common/index.mjs';

import assert from 'node:assert';
import { existsSync } from 'node:fs';
import fs from 'node:fs/promises';
import path from 'node:path';

// This tests that `make doc` generates the JSON documentation properly.
// Note that for this test to pass, `make doc` must be run first.

if (common.isWindows) {
  common.skip('`make doc` does not run on Windows');
}

function validateModule(module) {
  assert.strictEqual(typeof module, 'object');
  assert.strictEqual(module.type, 'module');
  assert.ok(module.name);
  assert.ok(module.textRaw);
}

function validateMisc(misc) {
  assert.strictEqual(typeof misc, 'object');
  assert.strictEqual(misc.type, 'misc');
  assert.ok(misc.name);
  assert.strictEqual(typeof misc.name, 'string');
  assert.ok(misc.textRaw);
  assert.strictEqual(typeof misc.textRaw, 'string');
}

let numberOfDeprecatedSections = 0;
let numberOfRemovedAPIs = 0;

const metaExpectedKeys = new Set([
  'added',
  'changes',
  'deprecated',
  'napiVersion',
  'removed',
]);

function validateMeta(meta) {
  assert.partialDeepStrictEqual(metaExpectedKeys, new Set(Object.keys(meta)));
  assert.ok(!Object.hasOwn(meta, 'added') || Array.isArray(meta.added) || typeof meta.added === 'string');
  if (meta.deprecated) {
    numberOfDeprecatedSections++;
    assert.ok(Array.isArray(meta.deprecated) || typeof meta.deprecated === 'string');
  }
  if (meta.removed) {
    numberOfRemovedAPIs++;
    assert.ok(Array.isArray(meta.removed) || typeof meta.removed === 'string');
  }

  assert.ok(!Object.hasOwn(meta, 'napiVersion') || Number(meta.napiVersion));
  assert.ok(Array.isArray(meta.changes));
}

function findAllKeys(obj, allKeys = new Set()) {
  for (const [key, value] of Object.entries(obj)) {
    if (Number.isNaN(Number(key))) allKeys.add(key);
    if (typeof value === 'object') findAllKeys(value, allKeys);

    if (key === 'miscs') {
      assert.ok(Array.isArray(value));
      assert.ok(value.length);
      value.forEach(validateMisc);
    } else if (key === 'modules') {
      assert.ok(Array.isArray(value));
      assert.ok(value.length);
      value.forEach(validateModule);
    } else if (key === 'meta') {
      validateMeta(value);
    }
  }
  return allKeys;
}

const allExpectedKeys = new Set([
  'added',
  'changes',
  'classes',
  'classMethods',
  'commit',
  'ctors',
  'default',
  'deprecated',
  'desc',
  'description',
  'displayName',
  'events',
  'examples',
  'globals',
  'introduced_in',
  'meta',
  'methods',
  'miscs',
  'modules',
  'name',
  'napiVersion',
  'options',
  'params',
  'pr-url',
  'properties',
  'removed',
  'return',
  'shortDesc',
  'signatures',
  'source',
  'stability',
  'stabilityText',
  'textRaw',
  'type',
  'version',
]);

for await (const dirent of await fs.opendir(new URL('../../out/doc/api/', import.meta.url))) {
  if (!dirent.name.endsWith('.md')) continue;

  const jsonPath = path.join(dirent.parentPath, dirent.name.slice(0, -2) + 'json');
  const expectedSource = `doc/api/${dirent.name}`;
  if (dirent.name === 'quic.md') {
    assert.ok(!existsSync(jsonPath)); // QUIC documentation is not public yet
    continue;
  }

  console.log('testing', jsonPath, 'based on', expectedSource);

  const fileContent = await fs.readFile(jsonPath, 'utf8');
  // A proxy to check if the file is human readable is to count if it contains
  // at least 3 line return.
  assert.strictEqual(fileContent.split('\n', 3).length, 3);

  const json = JSON.parse(fileContent);

  assert.strictEqual(json.type, 'module');
  assert.strictEqual(json.source, expectedSource);
  if (dirent.name !== 'index.md') {
    assert.ok(json.introduced_in || Object.values(json).at(-1)?.[0].introduced_in);
  }
  assert.deepStrictEqual(Object.keys(json), ['type', 'source', ...({
    'addons.md': ['introduced_in', 'miscs'],
    'cli.md': ['introduced_in', 'miscs'],
    'debugger.md': ['introduced_in', 'stability', 'stabilityText', 'miscs'],
    'deprecations.md': ['introduced_in', 'miscs'],
    'documentation.md': ['introduced_in', 'miscs'],
    'errors.md': ['introduced_in', 'classes', 'miscs'],
    'esm.md': ['introduced_in', 'meta', 'stability', 'stabilityText', 'properties', 'miscs'],
    'globals.md': ['introduced_in', 'stability', 'stabilityText', 'classes', 'methods', 'miscs'],
    'index.md': [],
    'intl.md': ['introduced_in', 'miscs'],
    'n-api.md': ['introduced_in', 'stability', 'stabilityText', 'miscs'],
    'packages.md': ['introduced_in', 'meta', 'miscs'],
    'process.md': ['globals'],
    'report.md': ['introduced_in', 'meta', 'stability', 'stabilityText', 'miscs'],
    'synopsis.md': ['introduced_in', 'miscs'],
  }[dirent.name] ?? ['modules'])]);

  assert.partialDeepStrictEqual(allExpectedKeys, findAllKeys(json));
}

assert.strictEqual(numberOfDeprecatedSections, 39); // Increase this number every time a new API is deprecated.
assert.strictEqual(numberOfRemovedAPIs, 46); // Increase this number every time a section is marked as removed.
