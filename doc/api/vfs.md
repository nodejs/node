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
that resolve outside that root, but that check is not a security boundary.
[`ZipProvider`][] has no real file-system paths of its own to escape; its
entries only ever exist within the archive's own namespace. Do not rely on VFS
to run untrusted code; use operating-system-level isolation, such as separate
users, containers, or platform sandboxes, when a security boundary is
required.

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
call [`vfs.mount()`][]; call [`vfs.unmount()`][] (or rely on a
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

### `vfs.mount()`

<!-- YAML
added: REPLACEME
-->

* Returns: {string} The absolute mount point.

Mounts the virtual file system and returns the resulting mount point.
After mounting, files in the VFS can be accessed through the
`node:fs` module and resolved through `require()` and `import`
using paths under the returned mount point.

Mount points always live inside a reserved namespace that cannot have child file system entries,
so virtual paths never conflate with (or shadow) real paths. The virtual path scheme is subject to
change and users should not manually construct them based on assumptions. Instead, obtain
them from what `vfs.mount()` returns or `vfs.mountPoint`.

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

const myVfs = vfs.create();
myVfs.writeFileSync('/data.txt', 'Hello');
const mountPoint = myVfs.mount();
// e.g. '/dev/null/vfs/0'

fs.readFileSync(`${mountPoint}/data.txt`, 'utf8'); // 'Hello'
```

Each `VirtualFileSystem` instance may be mounted at most once at a
time. Attempting to mount an already-mounted instance throws
`ERR_INVALID_STATE`. Because each instance mounts inside its own
per-layer namespace, mounts from different instances can never
overlap.

The VFS supports the [Explicit Resource Management][] proposal. Use
a `using` declaration to unmount automatically when leaving scope:

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

let mountPoint;
{
  using myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'Hello');
  mountPoint = myVfs.mount();

  fs.readFileSync(`${mountPoint}/data.txt`, 'utf8'); // 'Hello'
} // VFS is automatically unmounted here

fs.existsSync(`${mountPoint}/data.txt`); // false
```

### `vfs.unmount()`

<!-- YAML
added: REPLACEME
-->

Unmounts the virtual file system. After unmounting, virtual files
are no longer reachable through `node:fs`, `require()`, or `import`.
The same instance may be mounted again by calling `mount()`.

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

The current mount point as an absolute string (the value returned by
the last [`vfs.mount()`][] call), or `null` when the VFS is not
mounted.

### `vfs.mountPointURL`

<!-- YAML
added: REPLACEME
-->

* {string | null}

The current mount point as a `file:` URL string (the [`vfs.mountPoint`][]
path converted with [`url.pathToFileURL()`][]), or `null` when the VFS
is not mounted.

This is a convenience for addressing mounted files with URL-based
APIs such as dynamic `import()`:

```mjs
import vfs from 'node:vfs';

const myVfs = vfs.create();
myVfs.writeFileSync('/mod.mjs', 'export const value = 42;');
myVfs.mount();

const { value } = await import(`${myVfs.mountPointURL}/mod.mjs`);
console.log(value); // 42

myVfs.unmount();
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
* `lchownSync(path, uid, gid)`
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

Once a `VirtualFileSystem` is mounted, paths under the mount point
participate in module resolution and loading. The [CommonJS
resolution algorithm][] used by [`require()`][] and
[`require.resolve()`][] and the [ES modules resolution algorithm][]
used by `import` and [`import.meta.resolve()`][] are unchanged;
instead, every file system operation those algorithms perform is
dispatched on the path being probed: paths under a mount point are
served by the owning VFS, and all other paths are served by the real
file system. Files served from the VFS therefore behave as
first-class modules.

Because mounted paths live in a reserved namespace that cannot exist
on disk, any given path is served either by exactly one VFS or by
the real file system, never both. There is no search order or
fallback between the two: if a path under a mount point does not
exist in the VFS, resolution fails with `ENOENT` without consulting
the disk, and a mounted layer never shadows a real directory.

For resolution purposes the mount point behaves as a file system
root: `package.json` scope lookups and [loading from `node_modules`
folders][] stop at the mount point. For example, when
`${mountPoint}/foo/bar/main.cjs` calls `require('baz')`, the lookup
goes through:

* `${mountPoint}/foo/bar/node_modules/baz`
* `${mountPoint}/foo/node_modules/baz`
* `${mountPoint}/node_modules/baz`
* If `$NODE_PATH` is set, the folders listed in `$NODE_PATH`
* `$HOME/.node_modules/baz`
* `$HOME/.node_libraries/baz`
* `$PREFIX/lib/node/baz`

The last four entries are [the global folders][], which are legacy
CommonJS behavior and do not apply to `import`. Absolute specifiers
may cross the boundary in either direction: a module on the real
file system can `require()` a mounted path, and a virtual module can
`require()` a real one.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/lib');
myVfs.writeFileSync('/lib/greet.js', 'module.exports = () => "hi";');
myVfs.writeFileSync(
  '/lib/package.json', '{"main": "./greet.js"}');
const mountPoint = myVfs.mount();

const greet = require(`${mountPoint}/lib`);
console.log(greet()); // 'hi'

myVfs.unmount();
```

For ECMAScript modules, use `file:` URLs when passing mounted paths
to dynamic `import()`. [`vfs.mountPointURL`][] provides the mount
point in that form; this keeps VFS imports portable on Windows,
where mounted paths use Windows path syntax.

```mjs
import vfs from 'node:vfs';

const myVfs = vfs.create();
myVfs.writeFileSync('/mod.mjs', 'export const value = 42;');
myVfs.mount();

const { value } = await import(`${myVfs.mountPointURL}/mod.mjs`);
console.log(value); // 42

myVfs.unmount();
```

CommonJS modules loaded from a mounted VFS are identified by their VFS paths
that start with the mount point. This is reflected in, for example, `__filename` and
`__dirname` in the module, or the errors stack traces involving functions from
the VFS modules. ES modules in the VFS are similarly identified by the `file:` URL of
their VFS paths and this is reflected in e.g. `import.meta.url`.

Like modules loaded from the real file system, modules loaded from the VFS are
cached on the first load. When `require()` or `import()` is used to load an absolute
path or URL that falls under the mounted VFS multiple times, the module is only loaded
once and subsequent calls return the same instance.

Calling [`vfs.unmount()`][] invalidates the modules that were loaded
from the mount point: a subsequent `require()` or `import` of a path
under a re-created mount re-reads the file from the newly mounted
VFS rather than returning a stale module. Modules loaded from other
VFS instances or from the real file system are unaffected.

Mounting and unmounting do not stop any module execution that is
already started, or invalidate any objects materialized from VFS
modules that are already executed. As with modules in the real file
system, the callers are responsible for avoiding removal or
invalidation of modules in the virtual file system while they are
being loaded.

## Class: `VirtualProvider`

<!-- YAML
added: v26.4.0
-->

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

## Class: `ZipProvider`

<!-- YAML
added: REPLACEME
-->

A provider that exposes the entries of a ZIP archive - either a
[`zlib.ZipBuffer`][] (in memory) or a [`zlib.ZipFile`][] (on disk) - through
the VFS API. `provider.readonly` reflects the archive's own
[`zipFile.writable`][] flag: a `ZipBuffer` is always writable, and a
`ZipFile` is writable only when opened with `{ writable: true }`.

Directories are recognized both explicitly (an entry whose name ends in `/`)
and implicitly (any entry name starting with `"<dir>/"`). `readdir()` does
not support `{ recursive: true }`. Because a ZIP member cannot be edited or
read in place - only fully written or fully decompressed - a file opened for
writing only commits its content (as a new archive entry) when the handle is
closed.

Every method has a synchronous counterpart (`openSync()`, `statSync()`,
`readdirSync()`, and so on), backed by the equally complete synchronous
surface [`zlib.ZipBuffer`][]/[`zlib.ZipFile`][] expose. As with those, the
synchronous methods here block the Node.js event loop and further JavaScript
execution until the operation - including any deflate/inflate pass -
completes.

```cjs
const vfs = require('node:vfs');
const zlib = require('node:zlib');
const { readFileSync } = require('node:fs');

async function main() {
  const zip = new zlib.ZipBuffer(readFileSync('archive.zip'));
  const archiveVfs = vfs.create(new vfs.ZipProvider(zip));

  console.log(await archiveVfs.promises.readdir('/'));
  await archiveVfs.promises.writeFile('/new.txt', 'hello');
}
main();
```

### `new ZipProvider(source)`

<!-- YAML
added: REPLACEME
-->

* `source` {zlib.ZipBuffer|zlib.ZipFile} An already-open archive.

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

[CommonJS resolution algorithm]: modules.md#all-together
[ES modules resolution algorithm]: esm.md#resolution-algorithm
[Explicit Resource Management]: https://github.com/tc39/proposal-explicit-resource-management
[`MemoryProvider`]: #class-memoryprovider
[`RealFSProvider`]: #class-realfsprovider
[`VirtualFileSystem`]: #class-virtualfilesystem
[`VirtualProvider`]: #class-virtualprovider
[`ZipProvider`]: #class-zipprovider
[`fs.BigIntStats`]: fs.md#class-fsstats
[`fs.Stats`]: fs.md#class-fsstats
[`import.meta.resolve()`]: esm.md#importmetaresolvespecifier
[`node:fs`]: fs.md
[`require()`]: modules.md#requireid
[`require.resolve()`]: modules.md#requireresolverequest-options
[`url.pathToFileURL()`]: url.md#urlpathtofileurlpath-options
[`vfs.mount()`]: #vfsmount
[`vfs.mountPointURL`]: #vfsmountpointurl
[`vfs.mountPoint`]: #vfsmountpoint
[`vfs.unmount()`]: #vfsunmount
[`zipFile.writable`]: zlib.md#zipfilewritable
[`zlib.ZipBuffer`]: zlib.md#class-zlibzipbuffer
[`zlib.ZipFile`]: zlib.md#class-zlibzipfile
[loading from `node_modules` folders]: modules.md#loading-from-node_modules-folders
[the global folders]: modules.md#loading-from-the-global-folders
