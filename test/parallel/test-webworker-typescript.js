// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
if (!process.config.variables.node_use_amaro) {
  common.skip('Requires Amaro');
}

// Strip file entry types without changing the worker's module semantics.
const assert = require('node:assert');
const { mkdirSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');
const { pathToFileURL } = require('node:url');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

function createEntry(name, source) {
  const path = join(tmpdir.path, name);
  writeFileSync(path, source);
  return pathToFileURL(path);
}

function expectMessage(url, expected) {
  const worker = new Worker(url, { type: 'module' });
  worker.onerror = common.mustNotCall('worker failed');
  worker.onmessage = common.mustCall(({ data }) => {
    assert.deepStrictEqual(data, expected);
    worker.terminate();
  });
}

function expectError(url, code, type = 'module') {
  const worker = new Worker(url, { type });
  worker.onmessage = common.mustNotCall('worker unexpectedly succeeded');
  worker.onerror = common.mustCall(({ error }) => {
    assert.strictEqual(error.code ?? error.name, code);
  });
}

const entry = fixtures.fileURL('web-worker', 'typescript', 'entry.ts');
expectMessage(entry, 1);
expectError(entry, 'SyntaxError', 'classic');

// Worker type takes precedence over both the extension and package type.
writeFileSync(join(tmpdir.path, 'package.json'), '{ "type": "commonjs" }');
for (const extension of ['ts', 'mts', 'cts']) {
  const url = createEntry(`entry.${extension}`, `
    const value: number = 1;
    postMessage([value, typeof require, this === undefined, import.meta.main]);
  `);
  expectMessage(url, [1, 'undefined', true, true]);
}

// The stripped entry can still import TypeScript and keeps its original URL.
{
  const url = fixtures.fileURL('web-worker', 'typescript', 'module.ts');
  url.search = '?version=1';
  url.hash = '#entry';
  expectMessage(url, {
    value: 42,
    url: url.href,
    main: true,
    requireType: 'undefined',
    thisIsUndefined: true,
  });
}

// Extension detection and the node_modules restriction use the decoded path.
{
  const url = createEntry('space \u00e9.ts', 'const value: number = 1; postMessage(value);');
  expectMessage(url.href.replace('.ts', '.%74s') + '?version=2#entry', 1);
}
mkdirSync(join(tmpdir.path, 'node_modules'));
const dependency = createEntry('node_modules/entry.ts', 'postMessage(1);');
for (const url of [dependency.href, dependency.href.replace('node_modules', '%6eode_modules')]) {
  expectError(url, 'ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING');
}

// Type-stripping errors must reach the parent's error handler. JavaScript
// entries must not acquire TypeScript support.
expectError(createEntry('invalid.ts', 'const value: = 1;'), 'ERR_INVALID_TYPESCRIPT_SYNTAX');
expectError(createEntry('enum.ts', 'enum Value { A }'), 'ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX');
expectError(createEntry('entry.js', 'const value: number = 1;'), 'SyntaxError');
