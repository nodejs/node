'use strict';

// Test that readdirSync works with Buffer paths in recursive mode.
// Refs: https://github.com/nodejs/node/issues/58892

require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('node:assert');
const { readdirSync, mkdirSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');

tmpdir.refresh();

const subdir = join(tmpdir.path, 'subdir');
mkdirSync(subdir);
writeFileSync(join(tmpdir.path, 'file1.txt'), 'content1');
writeFileSync(join(subdir, 'file2.txt'), 'content2');

const result1 = readdirSync(Buffer.from(tmpdir.path), { recursive: true });
assert(Array.isArray(result1));
assert.strictEqual(result1.length, 3);

const result2 = readdirSync(Buffer.from(tmpdir.path), { recursive: true, withFileTypes: true });
assert(Array.isArray(result2));
assert.strictEqual(result2.length, 3);
