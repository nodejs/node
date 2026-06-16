# Virtual File System

<!--introduced_in=v26.4.0-->

<!-- YAML
added: v26.4.0
-->

> Stability: 1 - Experimental

<!-- source_link=lib/vfs.js -->

The `node:vfs` module provides a virtual file system with a `node:fs`-like API.
It is useful for tests, fixtures, embedded assets, and other scenarios where you
need a self-contained file system without touching the actual file-system.

To access it:

```mjs
import vfs from 'node:vfs';
```

```cjs
const vfs = require('node:vfs');
```

This module is only available under the `node:` scheme, and only when Node.js
is started with the `--experimental-vfs` flag.

## Security

The VFS API is not a sandbox, permission system, or access-control mechanism.
It does not isolate untrusted code from the host file system or from other
Node.js capabilities. Code that can access a [`VirtualFileSystem`][] instance,
mount it, select its provider, or pass paths to it is trusted application code.

Mounting a VFS only redirects supported [`node:fs`][] calls whose resolved paths
are under the mount point. It does not prevent code from using other paths or
other Node.js APIs to access resources available to the process.
[`RealFSProvider`][] maps VFS paths under its configured root and rejects paths
that resolve outside that root, but that check is not a security boundary. Do
not rely on VFS to run untrusted code; use operating-system-level isolation,
such as separate users, containers, or platform sandboxes, when a security
boundary is required.

## Basic usage

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/dir', { recursive: true });
myVfs.writeFileSync('/dir/hello.txt', 'Hello, VFS!');

console.log(myVfs.readFileSync('/dir/hello.txt', 'utf8')); // 'Hello, VFS!'
```

`vfs.create()` returns a [`VirtualFileSystem`][] instance backed by a
[`MemoryProvider`][] by default. The instance exposes synchronous,
callback-based, and promise-based file system methods that mirror the
shape of the [`node:fs`][] API. All paths are POSIX-style and absolute
(starting with `/`).

By default, the file tree is private to the VFS instance. To expose
it through the global `node:fs` module, `require()`, and `import`,
call [`vfs.mount(prefix)`][]; call [`vfs.unmount()`][] (or rely on a
`using` declaration) to detach again.

## `vfs.create([provider][, options])`

<!-- YAML
added: v26.4.0
-->

* `provider` {VirtualProvider} The provider to use. **Default:**
  `new MemoryProvider()`.
* `options` {Object}
  * `emitExperimentalWarning` {boolean} Whether to emit the experimental
    warning when the instance is created. **Default:** `true`.
* Returns: {VirtualFileSystem}

Convenience factory equivalent to `new VirtualFileSystem(provider, options)`.

```cjs
const vfs = require('node:vfs');

// Default in-memory provider
const memoryVfs = vfs.create();

// Explicit provider
const realVfs = vfs.create(new vfs.RealFSProvider('/tmp/vfs-root'));
```

## Class: `VirtualFileSystem`

<!-- YAML
added: v26.4.0
-->

A `VirtualFileSystem` wraps a [`VirtualProvider`][] and exposes a
`node:fs`-like API. Each instance maintains its own file tree.

### `new VirtualFileSystem([provider][, options])`

<!-- YAML
added: v26.4.0
-->

* `provider` {VirtualProvider} The provider to use. **Default:**
  `new MemoryProvider()`.
* `options` {Object}
  * `emitExperimentalWarning` {boolean} Whether to emit the experimental
    warning. **Default:** `true`.

### `vfs.mount(prefix)`

<!-- YAML
added: REPLACEME
-->

* `prefix` {string} The path prefix where the VFS will be mounted.
* Returns: {VirtualFileSystem} The VFS instance, for chaining or `using`.

Mounts the virtual file system at the specified path prefix. After
mounting, files in the VFS can be accessed through the `node:fs`
module — and resolved through `require()` and `import` — using paths
that start with the prefix.

If a real file-system path already exists at the mount prefix, the
VFS **shadows** that path: every operation against a path under the
mount point is directed to the VFS until the VFS is unmounted.

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

const myVfs = vfs.create();
myVfs.writeFileSync('/data.txt', 'Hello');
myVfs.mount('/virtual');

fs.readFileSync('/virtual/data.txt', 'utf8'); // 'Hello'
```

Each `VirtualFileSystem` instance may be mounted at most once at a
time. Attempting to mount an already-mounted instance throws
`ERR_INVALID_STATE`. Mounting two instances at overlapping prefixes
(e.g., `/virtual` and `/virtual/sub`) also throws `ERR_INVALID_STATE`.

The VFS supports the [Explicit Resource Management][] proposal. Use
a `using` declaration to unmount automatically when leaving scope:

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

{
  using myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'Hello');
  myVfs.mount('/virtual');

  fs.readFileSync('/virtual/data.txt', 'utf8'); // 'Hello'
} // VFS is automatically unmounted here

fs.existsSync('/virtual/data.txt'); // false
```

### `vfs.unmount()`

<!-- YAML
added: REPLACEME
-->

Unmounts the virtual file system. After unmounting, virtual files
are no longer reachable through `node:fs`, `require()`, or `import`.
The same instance may be mounted again, at the same or a different
prefix, by calling `mount()`.

This method is idempotent: calling `unmount()` on a VFS that is not
currently mounted has no effect.

### `vfs.mounted`

<!-- YAML
added: REPLACEME
-->

* {boolean}

`true` while the VFS is mounted; `false` otherwise.

### `vfs.mountPoint`

<!-- YAML
added: REPLACEME
-->

* {string | null}

The current mount-point path as an absolute string, or `null` when
the VFS is not mounted.

### `vfs.layerId`

<!-- YAML
added: REPLACEME
-->

* {number}

A per-process monotonically increasing identifier assigned at
construction. The id is stable across `mount()` / `unmount()` cycles
for the lifetime of the instance, and is independent of the order in
which VFS layers are mounted.

The layer id is the building block for cache scoping (see
[Module loader integration][]):

* it surfaces in `import.meta.url` for ES modules loaded from this
  VFS, as a `?vfs-layer=<id>` search parameter, so that the cascaded
  loader's caches can be scoped per VFS;
* it appears in the `NODE_DEBUG=vfs` output for `register` and
  `deregister` events;
* it appears in the `ERR_INVALID_STATE` error message thrown when two
  VFS instances try to mount at overlapping prefixes.

```cjs
const vfs = require('node:vfs');

const a = vfs.create();
const b = vfs.create();
console.log(a.layerId); // e.g. 0
console.log(b.layerId); // a.layerId + 1
```

### `vfs.provider`

<!-- YAML
added: v26.4.0
-->

* {VirtualProvider}

The provider backing this VFS instance.

### `vfs.readonly`

<!-- YAML
added: v26.4.0
-->

* {boolean}

`true` when the underlying provider is read-only.

### APIs

`VirtualFileSystem` implements the following methods, with the same
signatures as their [`node:fs`][] counterparts:

#### Synchronous API

* `existsSync(path)`
* `statSync(path[, options])`
* `lstatSync(path[, options])`
* `readFileSync(path[, options])`
* `writeFileSync(path, data[, options])`
* `appendFileSync(path, data[, options])`
* `readdirSync(path[, options])`
* `mkdirSync(path[, options])`
* `rmdirSync(path)`
* `unlinkSync(path)`
* `renameSync(oldPath, newPath)`
* `copyFileSync(src, dest[, mode])`
* `realpathSync(path[, options])`
* `readlinkSync(path[, options])`
* `symlinkSync(target, path[, type])`
* `accessSync(path[, mode])`
* `rmSync(path[, options])`
* `truncateSync(path[, len])`
* `ftruncateSync(fd[, len])`
* `linkSync(existingPath, newPath)`
* `chmodSync(path, mode)`
* `chownSync(path, uid, gid)`
* `utimesSync(path, atime, mtime)`
* `lutimesSync(path, atime, mtime)`
* `mkdtempSync(prefix)`
* `opendirSync(path[, options])`
* `openAsBlob(path[, options])`
* File-descriptor ops: `openSync`, `closeSync`, `readSync`, `writeSync`,
  `fstatSync`
* Streams: `createReadStream`, `createWriteStream`
* Watchers: `watch`, `watchFile`, `unwatchFile`

#### Callback API

`readFile`, `writeFile`, `stat`, `lstat`, `readdir`, `realpath`, `readlink`,
`access`, `open`, `close`, `read`, `write`, `rm`, `fstat`, `truncate`,
`ftruncate`, `link`, `mkdtemp`, `opendir`. Each takes a Node.js-style
callback `(err, ...result) => {}`.

#### Promise API

`vfs.promises` exposes the promise-based variants:

```cjs
const vfs = require('node:vfs');

async function example() {
  const myVfs = vfs.create();
  await myVfs.promises.writeFile('/file.txt', 'hello');
  const data = await myVfs.promises.readFile('/file.txt', 'utf8');
  return data;
}
example();
```

The promise namespace mirrors `fs.promises` and includes `readFile`,
`writeFile`, `appendFile`, `stat`, `lstat`, `readdir`, `mkdir`, `rmdir`,
`unlink`, `rename`, `copyFile`, `realpath`, `readlink`, `symlink`,
`access`, `rm`, `truncate`, `link`, `mkdtemp`, `chmod`, `chown`, `lchown`,
`utimes`, `lutimes`, `open`, `lchmod`, and `watch`.

## Module loader integration

Once a `VirtualFileSystem` is mounted, paths under the mount prefix
participate in module resolution and loading. Both
`require()` / `require.resolve()` (CommonJS) and `import` /
`import.meta.resolve()` (ECMAScript modules) consult the VFS through
the same toggleable hooks that `node:fs` uses, so files served from
the VFS are first-class modules: `package.json` is honoured,
extensionless files are sniffed for Wasm vs. JavaScript, conditional
`exports` / `imports` work, and so on.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/lib');
myVfs.writeFileSync('/lib/greet.js', 'module.exports = () => "hi";');
myVfs.writeFileSync(
  '/lib/package.json', '{"main": "./greet.js"}');
myVfs.mount('/virtual');

const greet = require('/virtual/lib');
console.log(greet()); // 'hi'

myVfs.unmount();
```

### Cache scoping and `import.meta.url`

Module loaders maintain caches that survive the lifetime of any
single VFS. To keep entries from leaking once a VFS is unmounted
without invalidating unrelated real-fs imports, two mechanisms are
combined:

* **CommonJS caches** (`require.cache`, the internal stat and
  realpath caches, and the `package.json` caches) are filtered on
  `unmount()`: entries whose absolute filename would be claimed by
  the VFS going away are deleted. `__filename` and `module.filename`
  are unchanged - they remain plain absolute paths.

* **ECMAScript module URLs** are tagged at resolve time. When the
  resolver determines that a path belongs to a mounted VFS, it
  appends `?vfs-layer=<id>` (where `<id>` is the owning instance's
  [`vfs.layerId`][]) to the resolved URL. The tag therefore appears
  in `import.meta.url` and in cache keys, and on `unmount()` the
  cascaded loader's caches drop just the entries that carry the tag
  for the unmounting layer.

```mjs
// inside /virtual/lib/greet.mjs after the VFS above is mounted
console.log(import.meta.url);
// e.g. 'file:///virtual/lib/greet.mjs?vfs-layer=0'
```

User code that compares `import.meta.url` literally should account
for the search parameter; use `new URL(import.meta.url).pathname` or
`fileURLToPath()` to obtain the underlying path.

Mounting and unmounting do not invalidate ESM modules that are
already executing. As with any other module-system teardown,
unmounting a VFS while the import graph below it is still loading is
the caller's responsibility to avoid.

## Class: `VirtualProvider`

<!-- YAML
added: v26.4.0
-->

The base class for all VFS providers. Subclasses implement the essential
primitives (`open`, `stat`, `readdir`, `mkdir`, `rmdir`, `unlink`,
`rename`, ...) and inherit default implementations of the derived
The base class for all VFS providers. Subclasses implement the essential
primitives (such as `open`, `stat`, `readdir`, `mkdir`, `rmdir`, `unlink`,
`rename`, etc.) and inherit default implementations of the derived
methods (such as `readFile`, `writeFile`, `exists`, `copyFile`, `access`, etc.).

### Capability flags

* `provider.readonly` {boolean} **Default:** `false`.
* `provider.supportsSymlinks` {boolean} **Default:** `false`.
* `provider.supportsWatch` {boolean} **Default:** `false`.

### Creating custom providers

```cjs
const { VirtualProvider } = require('node:vfs');

class StaticProvider extends VirtualProvider {
  get readonly() { return true; }

  statSync(path) { /* ... */ }
  openSync(path, flags) { /* ... */ }
  readdirSync(path, options) { /* ... */ }
  // ...
}
```

The base class throws `ERR_METHOD_NOT_IMPLEMENTED` for any primitive
that has not been overridden, and rejects writes from a `readonly`
provider with `EROFS`.

## Class: `MemoryProvider`

<!-- YAML
added: v26.4.0
-->

The default in-memory provider. Stores files, directories, and symbolic
links in a `Map`-backed tree, supports symlinks (`supportsSymlinks ===
true`), and supports watching (`supportsWatch === true`).

### `memoryProvider.setReadOnly()`

<!-- YAML
added: v26.4.0
-->

Locks the provider into read-only mode. Subsequent writes through any
[`VirtualFileSystem`][] using this provider throw `EROFS`. There is no
way to revert the provider to writable.

```cjs
const vfs = require('node:vfs');

const provider = new vfs.MemoryProvider();
const myVfs = vfs.create(provider);
myVfs.writeFileSync('/seed.txt', 'initial');

provider.setReadOnly();

myVfs.writeFileSync('/x.txt', 'fail'); // throws EROFS
```

## Class: `RealFSProvider`

<!-- YAML
added: v26.4.0
-->

A provider that wraps a directory (i.e. one on the actual file system) and
exposes its contents through the VFS API. All VFS paths are resolved relative to
the root and verified to stay inside it; symbolic links resolving outside the
root are rejected. This path mapping is not a sandbox or access-control
mechanism.

### `new RealFSProvider(rootPath)`

<!-- YAML
added: v26.4.0
-->

* `rootPath` {string} The absolute file-system path to use as the root.
  Must be a non-empty string.

```cjs
const vfs = require('node:vfs');

const realVfs = vfs.create(new vfs.RealFSProvider('/tmp/vfs-root'));
realVfs.writeFileSync('/file.txt', 'hello'); // writes /tmp/vfs-root/file.txt
```

### `realFSProvider.rootPath`

<!-- YAML
added: v26.4.0
-->

* {string}

The resolved absolute path used as the root.

## Implementation details

### `Stats` objects

VFS `Stats` objects are real instances of [`fs.Stats`][] (or
[`fs.BigIntStats`][] when `{ bigint: true }` is requested). Their
fields use synthetic but stable values:

* `dev` is `4085` (the VFS device id).
* `ino` is monotonically increasing per process.
* `blksize` is `4096`.
* `blocks` is `Math.ceil(size / 512)`.
* Times default to the moment the entry was created/last modified.

[Explicit Resource Management]: https://github.com/tc39/proposal-explicit-resource-management
[Module loader integration]: #module-loader-integration
[`MemoryProvider`]: #class-memoryprovider
[`RealFSProvider`]: #class-realfsprovider
[`VirtualFileSystem`]: #class-virtualfilesystem
[`VirtualProvider`]: #class-virtualprovider
[`fs.BigIntStats`]: fs.md#class-fsbigintstats
[`fs.Stats`]: fs.md#class-fsstats
[`node:fs`]: fs.md
[`vfs.layerId`]: #vfslayerid
[`vfs.mount(prefix)`]: #vfsmountprefix
[`vfs.unmount()`]: #vfsunmount
