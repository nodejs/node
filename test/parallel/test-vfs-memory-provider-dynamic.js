// Flags: --experimental-vfs --expose-internals
'use strict';

// MemoryProvider supports dynamic content providers and lazily-populated
// directories internally. These features have no public construction API,
// so we drive them directly through MemoryEntry / MemoryProvider.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');
const { MemoryProvider } = require('internal/vfs/providers/memory');

function getRoot(provider) {
  const symbols = Object.getOwnPropertySymbols(provider);
  const kRoot = symbols.find((s) => s.description === 'kRoot');
  assert.ok(kRoot, 'kRoot symbol expected on MemoryProvider');
  return provider[kRoot];
}

function makeFileEntry(prototypeFrom, contentProvider) {
  const t = Date.now();
  const fileEntry = { __proto__: Object.getPrototypeOf(prototypeFrom) };
  Object.assign(fileEntry, {
    type: 0,           // TYPE_FILE
    mode: 0o644,
    content: Buffer.alloc(0),
    contentProvider,
    children: null,
    target: null,
    populate: null,
    populated: true,
    nlink: 1,
    uid: 0,
    gid: 0,
    atime: t,
    mtime: t,
    ctime: t,
    birthtime: t,
  });
  fileEntry.isFile = prototypeFrom.isFile.bind(fileEntry);
  fileEntry.isDirectory = prototypeFrom.isDirectory.bind(fileEntry);
  fileEntry.isSymbolicLink = prototypeFrom.isSymbolicLink.bind(fileEntry);
  fileEntry.isDynamic = prototypeFrom.isDynamic.bind(fileEntry);
  fileEntry.getContentSync = prototypeFrom.getContentSync.bind(fileEntry);
  fileEntry.getContentAsync = prototypeFrom.getContentAsync.bind(fileEntry);
  return fileEntry;
}

// ===== Lazy-populated directory =====
{
  const provider = new MemoryProvider();
  const root = getRoot(provider);

  const dir = {
    __proto__: Object.getPrototypeOf(root),
    type: 1,           // TYPE_DIR
    mode: 0o755,
    children: new Map(),
    populate: (scoped) => {
      scoped.addFile('hello.txt', 'lazy hello');
      scoped.addFile('dyn.txt', () => 'dynamic-string');
      scoped.addDirectory('subdir', null);
      scoped.addSymlink('link.txt', '/lazy/hello.txt');
    },
    populated: false,
    nlink: 1,
    uid: 0,
    gid: 0,
  };
  const t = Date.now();
  dir.atime = t; dir.mtime = t; dir.ctime = t; dir.birthtime = t;
  dir.isFile = root.isFile.bind(dir);
  dir.isDirectory = root.isDirectory.bind(dir);
  dir.isSymbolicLink = root.isSymbolicLink.bind(dir);
  dir.isDynamic = root.isDynamic.bind(dir);
  dir.getContentSync = root.getContentSync.bind(dir);
  dir.getContentAsync = root.getContentAsync.bind(dir);
  root.children.set('lazy', dir);

  const myVfs = vfs.create(provider);

  // Reading the lazy directory triggers populate
  const entries = myVfs.readdirSync('/lazy');
  assert.deepStrictEqual(entries.sort(),
                         ['dyn.txt', 'hello.txt', 'link.txt', 'subdir']);

  // Static file in the lazy directory
  assert.strictEqual(myVfs.readFileSync('/lazy/hello.txt', 'utf8'),
                     'lazy hello');

  // Dynamic content provider returning a string (sync read)
  assert.strictEqual(myVfs.readFileSync('/lazy/dyn.txt', 'utf8'),
                     'dynamic-string');

  // Dynamic content provider via promises.readFile
  myVfs.promises.readFile('/lazy/dyn.txt', 'utf8').then(common.mustCall((s) => {
    assert.strictEqual(s, 'dynamic-string');
  }));
}

// ===== Dynamic content provider returning a Buffer =====
{
  const provider = new MemoryProvider();
  const root = getRoot(provider);
  root.children.set('buf-dyn.txt',
                    makeFileEntry(root, () => Buffer.from('buffer-content')));

  const myVfs = vfs.create(provider);
  assert.strictEqual(myVfs.readFileSync('/buf-dyn.txt', 'utf8'),
                     'buffer-content');
}

// ===== Async-only content provider: sync API throws ERR_INVALID_STATE =====
{
  const provider = new MemoryProvider();
  const root = getRoot(provider);
  root.children.set('async-only.txt',
                    makeFileEntry(root, async () => 'async-only'));

  const myVfs = vfs.create(provider);
  assert.throws(() => myVfs.readFileSync('/async-only.txt'),
                { code: 'ERR_INVALID_STATE' });

  myVfs.promises.readFile('/async-only.txt', 'utf8').then(common.mustCall((s) => {
    assert.strictEqual(s, 'async-only');
  }));
}
