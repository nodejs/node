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
    assert.strictEqual(data, expected);
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

// Worker type takes precedence over both the extension and package type.
writeFileSync(join(tmpdir.path, 'package.json'), '{ "type": "commonjs" }');
for (const extension of ['ts', 'mts', 'cts']) {
  const url = createEntry(`entry.${extension}`, 'const type: string = typeof require; postMessage(type);');
  expectMessage(url, 'undefined');
}

// The entry can import TypeScript, and a query or hash does not affect detection.
{
  const url = fixtures.fileURL('web-worker', 'typescript', 'module.ts');
  url.search = '?version=1';
  url.hash = '#entry';
  expectMessage(url, 42);
}

// Stripping errors reach the parent's error handler.
mkdirSync(join(tmpdir.path, 'node_modules'));
expectError(createEntry('node_modules/entry.ts', 'postMessage(1);'),
            'ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING');

// Classic workers and `.js` entries are not stripped.
expectError(fixtures.fileURL('web-worker', 'typescript', 'entry.ts'), 'SyntaxError', 'classic');
expectError(createEntry('entry.js', 'const value: number = 1;'), 'SyntaxError');
