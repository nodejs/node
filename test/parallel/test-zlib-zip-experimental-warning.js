'use strict';

// The zlib ZIP archive API is experimental and must warn when it is *used*,
// but importing or requiring node:zlib - or merely accessing a ZIP export,
// as the ESM loader does when building its named-export bindings - must not
// warn. See lib/zlib.js.

require('../common');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

const WARNING = /ExperimentalWarning: The zlib ZIP archive API/;

function run(...args) {
  return spawnSync(process.execPath, args, { encoding: 'utf8' });
}

test('importing node:zlib (ESM) does not emit the ZIP experimental warning', () => {
  const src = "import 'node:zlib';\n" +
    "import { ZipFile, ZipBuffer, createZipArchive } from 'node:zlib';\n" +
    'void ZipFile; void ZipBuffer; void createZipArchive;';
  const r = run('--input-type=module', '-e', src);
  assert.strictEqual(r.status, 0, r.stderr);
  assert.doesNotMatch(r.stderr, WARNING);
});

test('requiring node:zlib and only reading ZIP exports does not warn', () => {
  const src = "const z = require('zlib');\n" +
    'void z.ZipFile; void z.ZipEntry; void z.ZipBuffer; void z.createZipArchive;\n' +
    'void ({} instanceof z.ZipBuffer);'; // The instanceof check must not warn.
  const r = run('-e', src);
  assert.strictEqual(r.status, 0, r.stderr);
  assert.doesNotMatch(r.stderr, WARNING);
});

test('using a ZIP export emits the experimental warning (CJS)', () => {
  const r = run('-e', "require('zlib').createZipArchive([]);");
  assert.strictEqual(r.status, 0, r.stderr);
  assert.match(r.stderr, WARNING);
});

test('using a ZIP export emits the experimental warning (ESM)', () => {
  const src = "import { createZipArchive } from 'node:zlib';\ncreateZipArchive([]);";
  const r = run('--input-type=module', '-e', src);
  assert.strictEqual(r.status, 0, r.stderr);
  assert.match(r.stderr, WARNING);
});

test('instanceof against the public ZIP classes still matches real instances', () => {
  const src = "const z = require('zlib');\n" +
    'const bytes = Buffer.concat([...z.createZipArchiveSync([])]);\n' +
    'const buf = new z.ZipBuffer(bytes);\n' +
    'console.log(buf instanceof z.ZipBuffer, typeof z.ZipFile, z.ZipFile.name);';
  const r = run('-e', src);
  assert.strictEqual(r.status, 0, r.stderr);
  assert.match(r.stdout, /^true function ZipFile$/m);
});
