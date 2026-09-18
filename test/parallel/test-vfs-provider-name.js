// Flags: --experimental-vfs --expose-internals
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const vfs = require('node:vfs');
const { spawnSyncAndAssert } = require('../common/child_process');
const { selectProvider } = require('internal/vfs/provider_registry');

tmpdir.refresh();

function emptyZip() {
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync([])) chunks.push(chunk);
  return Buffer.concat(chunks);
}

// Every provider constructor takes a name, and has none by default.
{
  const zip = new zlib.ZipBuffer(emptyZip());
  const cases = [
    [(o) => new vfs.VirtualProvider(o)],
    [(o) => new vfs.MemoryProvider(o)],
    [(o) => new vfs.RealFSProvider(tmpdir.path, o)],
    [(o) => new vfs.ZipProvider(zip, o)],
  ];
  for (const [make] of cases) {
    assert.strictEqual(make().name, undefined);
    assert.strictEqual(make({}).name, undefined);
    assert.strictEqual(make({ name: 'named' }).name, 'named');
    assert.strictEqual(make({ name: '' }).name, '');
    assert.throws(() => make({ name: 1 }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => make(null), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => make('named'), { code: 'ERR_INVALID_ARG_TYPE' });
  }
}

// A subclass passes the name on through super().
{
  class Custom extends vfs.VirtualProvider {
    constructor(options) {
      super(options);
      this.extra = true;
    }
  }
  assert.strictEqual(new Custom({ name: 'custom' }).name, 'custom');
  assert.strictEqual(new Custom().name, undefined);
}

// A VirtualFileSystem exposes the name of its provider.
{
  const provider = new vfs.MemoryProvider({ name: 'mem' });
  const myVfs = vfs.create(provider);
  assert.strictEqual(myVfs.name, 'mem');
  assert.strictEqual(new vfs.VirtualFileSystem(provider).name, 'mem');
  assert.strictEqual(vfs.create().name, undefined);

  myVfs.mount();
  assert.strictEqual(myVfs.name, 'mem');
  myVfs.unmount();
}

// The built-in --vfs-mount providers are given the name.
{
  const dir = tmpdir.resolve('builtin-dir');
  fs.mkdirSync(dir);
  const dirProvider = selectProvider(dir, fs.statSync(dir), { name: 'd' });
  assert.ok(dirProvider instanceof vfs.RealFSProvider);
  assert.strictEqual(dirProvider.name, 'd');

  const zipPath = tmpdir.resolve('builtin.zip');
  fs.writeFileSync(zipPath, emptyZip());
  const zipProvider = selectProvider(zipPath, fs.statSync(zipPath), { name: 'z' });
  assert.ok(zipProvider instanceof vfs.ZipProvider);
  assert.strictEqual(zipProvider.name, 'z');
}

// --vfs-mount and --vfs-load take an optional `name=` prefix.
{
  // Reports the path and name each mount's provider is created with.
  const probe = tmpdir.resolve('probe.js');
  fs.writeFileSync(probe, `
'use strict';
const vfs = require('node:vfs');
vfs.registerProvider({
  name: 'probe',
  canHandle: (p, stats) => stats.isDirectory(),
  create(p, stats, options) {
    const provider = new vfs.RealFSProvider(p, options);
    console.log(JSON.stringify([p, provider.name ?? null]));
    return provider;
  },
});
`);

  const plain = tmpdir.resolve('plain');
  const withEq = tmpdir.resolve('x=y');
  fs.mkdirSync(plain);
  fs.mkdirSync(withEq);
  fs.writeFileSync(path.join(plain, 'index.js'),
                   'console.log(JSON.stringify(process.argv[1]));\n');

  function mounts(values, expected) {
    spawnSyncAndAssert(process.execPath, [
      '--experimental-vfs', '--no-warnings', '-r', probe,
      ...values.map((v) => `--vfs-mount=${v}`),
      '-e', '',
    ], { cwd: tmpdir.path }, {
      stdout(output) {
        const got = output.trim().split('\n').map((line) => JSON.parse(line));
        assert.deepStrictEqual(got, expected);
      },
    });
  }

  mounts([`assets=${plain}`], [[plain, 'assets']]);
  mounts([plain], [[plain, null]]);
  // Only the first `=` separates the name.
  mounts([`a=${withEq}`], [[withEq, 'a']]);
  // A prefix holding a path separator belongs to the source.
  mounts([withEq], [[withEq, null]]);
  mounts(['./x=y'], [[withEq, null]]);
  // An empty name is no name.
  mounts([`=${plain}`], [[plain, null]]);
  // Relative sources resolve after the name is removed.
  mounts(['n=plain'], [[plain, 'n']]);
  mounts([`one=${plain}`, `two=${withEq}`], [[plain, 'one'], [withEq, 'two']]);

  // --vfs-load names its mount the same way, and process.argv[1] reports
  // the source without the name.
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs', '--no-warnings', '-r', probe,
    `--vfs-load=app=${plain}`,
  ], {
    stdout(output) {
      const lines = output.trim().split('\n').map((line) => JSON.parse(line));
      assert.deepStrictEqual(lines, [[plain, 'app'], plain]);
    },
  });
}
