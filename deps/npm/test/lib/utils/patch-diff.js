const t = require('tap')
const { resolve } = require('node:path')
const { readFileSync, existsSync, symlinkSync } = require('node:fs')
const { diffDirs } = require('../../../lib/utils/patch-diff.js')
const { applyPatchToDir } = require('@npmcli/arborist/lib/patch.js')

// Helper to read a file from a dir as utf8.
const read = (...p) => readFileSync(resolve(...p), 'utf8')

t.test('modified file produces a unified diff', async t => {
  const dir = t.testdir({
    orig: { 'index.js': 'hello\n' },
    edit: { 'index.js': 'world\n' },
  })
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.match(diff, '--- a/index.js', 'has old header')
  t.match(diff, '+++ b/index.js', 'has new header')
  t.match(diff, '-hello', 'removes old line')
  t.match(diff, '+world', 'adds new line')
  t.notMatch(diff, '====', 'index separator is stripped')
})

t.test('added file uses --- /dev/null', async t => {
  const dir = t.testdir({
    orig: { 'keep.js': 'same\n' },
    edit: { 'keep.js': 'same\n', 'added.js': 'brand new\n' },
  })
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.match(diff, '--- /dev/null', 'old side is /dev/null')
  t.match(diff, '+++ b/added.js', 'new side names the added file')
  t.match(diff, '+brand new', 'includes added content')
  t.notMatch(diff, 'keep.js', 'identical file is not in the diff')
})

t.test('deleted file uses +++ /dev/null', async t => {
  const dir = t.testdir({
    orig: { 'gone.js': 'remove me\n' },
    edit: {},
  })
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.match(diff, '--- a/gone.js', 'old side names the deleted file')
  t.match(diff, '+++ /dev/null', 'new side is /dev/null')
  t.match(diff, '-remove me', 'includes removed content')
})

t.test('nested file path is posix-separated in the diff', async t => {
  const dir = t.testdir({
    orig: { lib: { deep: { 'x.js': 'a\n' } } },
    edit: { lib: { deep: { 'x.js': 'b\n' } } },
  })
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.match(diff, '--- a/lib/deep/x.js', 'old header uses posix separators')
  t.match(diff, '+++ b/lib/deep/x.js', 'new header uses posix separators')
})

t.test('identical files produce no diff', async t => {
  const dir = t.testdir({
    orig: { 'a.js': 'x\n', sub: { 'b.js': 'y\n' } },
    edit: { 'a.js': 'x\n', sub: { 'b.js': 'y\n' } },
  })
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.equal(diff, '', 'empty diff for identical trees')
})

t.test('node_modules and .git are ignored', async t => {
  const dir = t.testdir({
    orig: {
      'index.js': 'v1\n',
      node_modules: { dep: { 'index.js': 'old\n' } },
      '.git': { HEAD: 'ref: refs/heads/main\n' },
    },
    edit: {
      'index.js': 'v2\n',
      node_modules: { dep: { 'index.js': 'changed\n' } },
      '.git': { HEAD: 'ref: refs/heads/other\n' },
    },
  })
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.match(diff, 'index.js', 'top-level change is captured')
  t.notMatch(diff, 'node_modules', 'node_modules contents are excluded')
  t.notMatch(diff, 'HEAD', '.git contents are excluded')
})

t.test('root package.json is excluded and flagged, nested is kept', async t => {
  const dir = t.testdir({
    orig: {
      'package.json': '{ "version": "1.0.0" }\n',
      'index.js': 'a\n',
      sub: { 'package.json': '{ "private": true }\n' },
    },
    edit: {
      'package.json': '{ "version": "2.0.0" }\n',
      'index.js': 'b\n',
      sub: { 'package.json': '{ "private": false }\n' },
    },
  })
  const { diff, packageJsonChanged } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.equal(packageJsonChanged, true, 'root package.json change is flagged')
  t.notMatch(diff, 'a/package.json\t', 'root package.json is not in the diff')
  t.match(diff, 'a/sub/package.json', 'nested package.json is still diffed')
  t.match(diff, 'a/index.js', 'other files are still diffed')
})

t.test('packageJsonChanged is false when only other files change', async t => {
  const dir = t.testdir({
    orig: { 'package.json': '{ "version": "1.0.0" }\n', 'index.js': 'a\n' },
    edit: { 'package.json': '{ "version": "1.0.0" }\n', 'index.js': 'b\n' },
  })
  const { diff, packageJsonChanged } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.equal(packageJsonChanged, false, 'unchanged package.json is not flagged')
  t.match(diff, 'a/index.js', 'the real change is captured')
})

t.test('non-file entries like symlinks are skipped', async t => {
  const dir = t.testdir({
    orig: { 'real.js': 'a\n' },
    edit: { 'real.js': 'b\n' },
  })
  // A symlink is neither a directory nor a regular file so it is ignored.
  symlinkSync(resolve(dir, 'orig', 'real.js'), resolve(dir, 'edit', 'link.js'))
  const { diff } = await diffDirs(resolve(dir, 'orig'), resolve(dir, 'edit'))
  t.match(diff, 'real.js', 'regular file is diffed')
  t.notMatch(diff, 'link.js', 'symlink entry is skipped')
})

t.test('round-trip: applying the diff reproduces the edited tree', async t => {
  const dir = t.testdir({
    orig: {
      'mod.js': 'original line\n',
      'del.js': 'doomed\n',
      lib: { deep: { 'x.js': 'before\n' } },
    },
    edit: {
      'mod.js': 'patched line\n',
      'add.js': 'fresh content\n',
      lib: { deep: { 'x.js': 'after\n' } },
    },
  })
  const orig = resolve(dir, 'orig')
  const { diff } = await diffDirs(orig, resolve(dir, 'edit'))

  // Apply the diff back onto a copy of the original and check the result.
  await applyPatchToDir({ patch: diff, cwd: orig })

  t.equal(read(orig, 'mod.js'), 'patched line\n', 'modified file matches edit')
  t.equal(read(orig, 'add.js'), 'fresh content\n', 'added file was created')
  t.equal(read(orig, 'lib', 'deep', 'x.js'), 'after\n', 'nested file matches edit')
  t.notOk(existsSync(resolve(orig, 'del.js')), 'deleted file was removed')
})
