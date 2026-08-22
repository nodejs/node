// Regression test for https://github.com/nodejs/node/issues/59202
//
// `fs.glob*` APIs lacked an `encoding` option, so they decoded directory
// entries as UTF-8 and silently replaced invalid byte sequences (mojibake).
// With `encoding: 'buffer'`, the raw bytes of the file name must round-trip
// through `globSync`, `glob` (callback) and `fsPromises.glob`, in both
// path-string and `withFileTypes` modes.

import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { mkdir, writeFile, glob as asyncGlob } from 'node:fs/promises';
import { glob, globSync, Dirent } from 'node:fs';
import { test, describe } from 'node:test';
import { promisify } from 'node:util';
import { Buffer } from 'node:buffer';
import { sep } from 'node:path';
import assert from 'node:assert';

const promisifiedGlob = promisify(glob);

// Latin1 byte 0xE9 (`é`) is a stand-in for any non-ASCII byte. The point of
// this test is not the specific code unit but that `globSync` must not lose
// information when decoding a file name with `encoding: 'buffer'`.
const fileBuffer = Buffer.from([0x66, 0x6f, 0xe9, 0x2e, 0x74, 0x78, 0x74]); // "foé.txt"
const dirBuffer = Buffer.from([0x73, 0x75, 0x62, 0xe9]);                    // "subé"
const nestedBuffer = Buffer.from([0x62, 0x61, 0xe9, 0x72]);                 // "baér"

// On Windows, file names are UTF-16 internally — non-UTF-8 byte sequences
// can not be created on disk and the OS will reinterpret the bytes. Skip the
// non-UTF-8 portion of the test there. Pure ASCII Buffer round-tripping
// (which exercises the new option's plumbing) is still validated below.
const supportsNonUtf8 = !common.isWindows;

tmpdir.refresh();

const fixtureDir = tmpdir.resolve('glob-encoding');
await mkdir(fixtureDir, { recursive: true });

if (supportsNonUtf8) {
  // Place the byte-named file inside fixtureDir using a Buffer path so
  // Node's UTF-8 conversion never touches it.
  const filePath = Buffer.concat([
    Buffer.from(fixtureDir + sep),
    fileBuffer,
  ]);
  await writeFile(filePath, 'hello');

  const subDir = Buffer.concat([
    Buffer.from(fixtureDir + sep),
    dirBuffer,
  ]);
  await mkdir(subDir);
  const nestedFile = Buffer.concat([
    subDir,
    Buffer.from(sep),
    nestedBuffer,
  ]);
  await writeFile(nestedFile, 'world');
}

// Regardless of whether non-UTF-8 names can be created, also drop a regular
// ASCII file in the directory so the basic plumbing of the option is
// exercised on every platform.
await writeFile(`${fixtureDir}/plain.txt`, 'plain');

describe('fs.globSync with encoding option', () => {
  test('returns Buffer paths when encoding is "buffer"', () => {
    const matches = globSync('*', { cwd: fixtureDir, encoding: 'buffer' });
    for (const m of matches) {
      assert.ok(Buffer.isBuffer(m), `expected Buffer, got ${typeof m}`);
    }
    // The plain ASCII file must always be present and decodable as UTF-8.
    const names = matches.map((m) => m.toString('utf8'));
    assert.ok(names.includes('plain.txt'));
  });

  test('preserves non-UTF-8 bytes in returned Buffer paths', { skip: !supportsNonUtf8 }, () => {
    const matches = globSync('*', { cwd: fixtureDir, encoding: 'buffer' });
    const names = matches.map((m) => (Buffer.isBuffer(m) ? m : Buffer.from(m)));
    const found = names.some((n) => n.equals(fileBuffer));
    assert.ok(found, `expected to find ${fileBuffer.toString('hex')} ` +
      `in ${names.map((n) => n.toString('hex')).join(', ')}`);
  });

  test('returns Dirent with Buffer name and parentPath when withFileTypes', () => {
    const matches = globSync('*', {
      cwd: fixtureDir,
      encoding: 'buffer',
      withFileTypes: true,
    });
    for (const m of matches) {
      assert.ok(m instanceof Dirent);
      assert.ok(Buffer.isBuffer(m.name), `expected Dirent.name to be a Buffer, got ${typeof m.name}`);
      assert.ok(Buffer.isBuffer(m.parentPath), `expected Dirent.parentPath to be a Buffer, got ${typeof m.parentPath}`);
    }
  });

  test('preserves non-UTF-8 bytes in Dirent.name when withFileTypes', { skip: !supportsNonUtf8 }, () => {
    const matches = globSync('*', {
      cwd: fixtureDir,
      encoding: 'buffer',
      withFileTypes: true,
    });
    const names = matches.map((d) => d.name);
    const found = names.some((n) => n.equals(fileBuffer));
    assert.ok(found, `expected to find ${fileBuffer.toString('hex')} ` +
      `in ${names.map((n) => n.toString('hex')).join(', ')}`);
  });

  test('returns strings when encoding is omitted (existing behavior)', () => {
    const matches = globSync('*', { cwd: fixtureDir });
    for (const m of matches) {
      assert.strictEqual(typeof m, 'string');
    }
  });

  test('rejects unknown encodings', () => {
    assert.throws(() => globSync('*', { cwd: fixtureDir, encoding: 'not-an-encoding' }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  });

  test('walks into directories with non-UTF-8 names', { skip: !supportsNonUtf8 }, () => {
    const matches = globSync('**/*', { cwd: fixtureDir, encoding: 'buffer' });
    const names = matches.map((m) => m.toString('latin1'));
    const expectedNested = `${dirBuffer.toString('latin1')}/${nestedBuffer.toString('latin1')}`;
    assert.ok(names.some((n) => n === expectedNested),
              `expected to find nested entry ${expectedNested} in ${names.join(', ')}`);
  });
});

describe('fs.glob (callback) with encoding option', () => {
  test('returns Buffer paths when encoding is "buffer"', async () => {
    const matches = await promisifiedGlob('*', { cwd: fixtureDir, encoding: 'buffer' });
    for (const m of matches) {
      assert.ok(Buffer.isBuffer(m));
    }
  });

  test('preserves non-UTF-8 bytes in returned paths', { skip: !supportsNonUtf8 }, async () => {
    const matches = await promisifiedGlob('*', { cwd: fixtureDir, encoding: 'buffer' });
    const found = matches.some((n) => Buffer.isBuffer(n) && n.equals(fileBuffer));
    assert.ok(found);
  });
});

describe('fsPromises.glob with encoding option', () => {
  test('yields Buffer paths when encoding is "buffer"', async () => {
    const collected = [];
    for await (const item of asyncGlob('*', { cwd: fixtureDir, encoding: 'buffer' })) {
      collected.push(item);
    }
    for (const m of collected) {
      assert.ok(Buffer.isBuffer(m));
    }
  });

  test('yields Dirent with Buffer name when withFileTypes and encoding is "buffer"', async () => {
    const collected = [];
    for await (const item of asyncGlob('*', {
      cwd: fixtureDir,
      encoding: 'buffer',
      withFileTypes: true,
    })) {
      collected.push(item);
    }
    for (const d of collected) {
      assert.ok(d instanceof Dirent);
      assert.ok(Buffer.isBuffer(d.name));
    }
  });

  test('preserves non-UTF-8 bytes in yielded paths', { skip: !supportsNonUtf8 }, async () => {
    const collected = [];
    for await (const item of asyncGlob('*', { cwd: fixtureDir, encoding: 'buffer' })) {
      collected.push(item);
    }
    const found = collected.some((n) => Buffer.isBuffer(n) && n.equals(fileBuffer));
    assert.ok(found);
  });
});
