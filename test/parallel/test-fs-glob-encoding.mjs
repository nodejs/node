import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { join, sep } from 'node:path';
import { promisify } from 'node:util';
import { test } from 'node:test';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();
const cwd = tmpdir.path;
fs.mkdirSync(join(cwd, 'dir'));
fs.writeFileSync(join(cwd, 'dir', '文件.txt'), 'hello');

const apis = [
  ['sync', fs.globSync],
  ['callback', promisify(fs.glob)],
  ['iterator', (pattern, options) => Array.fromAsync(fs.promises.glob(pattern, options))],
];

// Encode independently of glob, starting with the bytes used to create files.
function encode(value, encoding) {
  const bytes = Buffer.from(value);
  return encoding === 'buffer' ? bytes : bytes.toString(encoding || 'utf8');
}

for (const [api, glob] of apis) {
  for (const encoding of [undefined, null, 'utf8', 'buffer', 'latin1', 'hex', 'base64', 'utf16le']) {
    await test(`${api}: ${encoding} paths and Dirents`, async () => {
      const options = { cwd, encoding };
      assert.deepStrictEqual(await glob('dir/*', options), [
        encode(join('dir', '文件.txt'), encoding),
      ]);
      const entries = await glob('dir/*', { ...options, withFileTypes: true });
      assert.strictEqual(entries.length, 1);
      assert(entries[0].isFile());
      assert.deepStrictEqual(entries[0].name, encode('文件.txt', encoding));
      assert.deepStrictEqual(entries[0].parentPath, encode(join(cwd, 'dir'), encoding));
    });
  }

  await test(`${api}: validates encoding`, async () => {
    for (const encoding of ['invalid', 123, {}]) {
      await assert.rejects(async () => glob('*', { cwd, encoding }), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    }
  });

  for (const withFileTypes of [false, true]) {
    for (const encoding of ['buffer', 'hex']) {
      await test(`${api}: ${encoding} exclude, withFileTypes=${withFileTypes}`, async () => {
        let seen = false;
        const results = await glob('dir/**', {
          cwd,
          encoding,
          withFileTypes,
          exclude: common.mustCallAtLeast((value) => {
            const name = withFileTypes ? value.name : value;
            assert.strictEqual(Buffer.isBuffer(name), encoding === 'buffer');
            if (withFileTypes) {
              assert.strictEqual(Buffer.isBuffer(value.parentPath), encoding === 'buffer');
            }
            if (name.toString() === encode('文件.txt', encoding).toString()) {
              seen = true;
              if (withFileTypes) assert(value.isFile());
              return true;
            }
            return false;
          }),
        });
        assert(seen);
        assert.deepStrictEqual(
          results.map((value) => (withFileTypes ? value.name : value)),
          [encode('dir', encoding)],
        );
      });
    }
  }

  await test(`${api}: relative cwd`, async () => {
    const options = { cwd: '.', encoding: 'buffer', withFileTypes: true };
    const [entry] = await glob('test/common', options);
    assert(entry.isDirectory());
    assert.deepStrictEqual(entry.name, Buffer.from('common'));
    assert.deepStrictEqual(entry.parentPath, Buffer.from('test'));
  });

  await test(`${api}: Windows root cwd`, { skip: !common.isWindows }, async () => {
    const name = fs.readdirSync('\\').find((name) => !/[*?[\]{}()!]/.test(name));
    assert(name);
    for (const encoding of ['utf8', 'buffer']) {
      const [entry] = await glob(name, { cwd: '\\', encoding, withFileTypes: true });
      assert.deepStrictEqual(entry.name, encode(name, encoding));
      assert.deepStrictEqual(entry.parentPath, encode('\\', encoding));
    }
  });
}

await test('async iterator preserves encoding across batches', async () => {
  const dir = join(cwd, 'batch');
  fs.mkdirSync(dir);
  const expected = new Map();
  // A native batch can consume a whole directory before it yields.
  for (let batch = 0; batch < 3; batch++) {
    const parent = join(dir, `${batch}`);
    fs.mkdirSync(parent);
    for (let i = 0; i < 150; i++) {
      const name = `${batch}-${i}.txt`;
      fs.writeFileSync(join(parent, name), '');
      expected.set(name, parent);
    }
  }
  const entries = await Array.fromAsync(fs.promises.glob('*/*', {
    cwd: dir, encoding: 'buffer', withFileTypes: true,
  }));
  assert.strictEqual(entries.length, expected.size);
  assert.strictEqual(new Set(entries.map((entry) => entry.name.toString())).size, expected.size);
  for (const entry of entries) {
    assert(Buffer.isBuffer(entry.name));
    assert.deepStrictEqual(entry.parentPath, Buffer.from(expected.get(entry.name.toString())));
  }
});

await test('non-UTF8 filenames survive traversal, filtering and reading', {
  skip: !common.isLinux && 'Requires a filesystem accepting non-UTF8 names',
}, async () => {
  const root = join(cwd, 'raw');
  fs.mkdirSync(root);
  const prefix = Buffer.from(`${root}/`);
  const names = [Buffer.from([0xe9]), Buffer.from([0xea])];
  for (const name of names) {
    const dir = Buffer.concat([prefix, name]);
    fs.mkdirSync(dir);
    fs.writeFileSync(Buffer.concat([dir, Buffer.from('/'), name]), name);
  }

  for (const [api, glob] of apis) {
    const expected = names.map((name) => Buffer.concat([name, Buffer.from('/'), name]));
    assert.deepStrictEqual(
      (await glob('[^a-z]', { cwd: root, encoding: 'buffer' })).sort(Buffer.compare),
      names,
      api,
    );
    assert.deepStrictEqual(
      (await glob('*/*', { cwd: root, encoding: 'buffer' })).sort(Buffer.compare),
      expected,
      api,
    );
    const entries = await glob('**/*', {
      cwd: root, encoding: 'buffer', withFileTypes: true,
    });
    assert.strictEqual(entries.length, 4, api);
    for (const entry of entries.filter((entry) => entry.isFile())) {
      const path = Buffer.concat([entry.parentPath, Buffer.from(sep), entry.name]);
      assert.deepStrictEqual(fs.readFileSync(path), entry.name, api);
    }
    const paths = await glob('**/*', {
      cwd: root,
      encoding: 'buffer',
      exclude: (name) => name.equals(names[0]),
    });
    assert.deepStrictEqual(paths.sort(Buffer.compare), [names[1], expected[1]], api);
    const filtered = await glob('**/*', {
      cwd: root,
      encoding: 'buffer',
      withFileTypes: true,
      exclude: (entry) => entry.name.equals(names[0]),
    });
    assert.strictEqual(filtered.length, 2, api);
    assert(filtered.every((entry) => entry.name.equals(names[1])), api);
  }

  // Path callbacks use lstat rather than a directory entry's cached type.
  // In particular, a hex-encoded display path must never be used for lstat.
  const originalCwd = process.cwd();
  process.chdir(root);
  try {
    for (const [api, glob] of apis) {
      let sawPath = false;
      const entries = await glob('**/*', {
        encoding: 'hex',
        withFileTypes: true,
        exclude: common.mustCallAtLeast((entry) => {
          if (entry.parentPath === '2e') {
            sawPath = true;
            assert(entry.isDirectory());
            assert(['e9', 'ea'].includes(entry.name));
          }
          return false;
        }),
      });
      assert(sawPath, api);
      assert.strictEqual(entries.length, 4, api);
    }
  } finally {
    process.chdir(originalCwd);
  }
});
