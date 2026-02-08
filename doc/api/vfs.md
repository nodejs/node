# Virtual File System

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

<!-- source_link=lib/vfs.js -->

The `node:vfs` module provides a virtual file system that can be mounted
alongside the real file system. Virtual files can be read using standard `fs`
operations and loaded as modules using `require()` or `import`.

To access it:

```mjs
import vfs from 'node:vfs';
```

```cjs
const vfs = require('node:vfs');
```

This module is only available under the `node:` scheme.

## Overview

The Virtual File System (VFS) allows you to create in-memory file systems that
integrate seamlessly with the Node.js `fs` module and module loading system. This
is useful for:

* Bundling assets in Single Executable Applications (SEA)
* Testing file system operations without touching the disk
* Creating virtual module systems
* Embedding configuration or data files in applications

## Operating modes

The VFS supports two operating modes:

### Standard mode (default)

When mounted at a path prefix (e.g., `/virtual`), the VFS handles **all**
operations for paths starting with that prefix. The VFS completely shadows
any real file system paths under the mount point.

### Overlay mode

When created with `{ overlay: true }`, the VFS selectively intercepts only
paths that exist within the VFS. Paths that don't exist in the VFS fall through
to the real file system. This is useful for mocking specific files while leaving
others unchanged.

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

// Overlay mode: only intercept files that exist in VFS
const myVfs = vfs.create({ overlay: true });
myVfs.writeFileSync('/etc/config.json', '{"mocked": true}');
myVfs.mount('/');

// This reads from VFS (file exists in VFS)
fs.readFileSync('/etc/config.json', 'utf8'); // '{"mocked": true}'

// This reads from real FS (file doesn't exist in VFS)
fs.readFileSync('/etc/hostname', 'utf8'); // Real file content
```

See [Security considerations][] for important warnings about overlay mode.

## Basic usage

The following example shows how to create a virtual file system, add files,
and access them through the standard `fs` API:

```mjs
import vfs from 'node:vfs';
import fs from 'node:fs';

// Create a new virtual file system
const myVfs = vfs.create();

// Create directories and files
myVfs.mkdirSync('/app');
myVfs.writeFileSync('/app/config.json', JSON.stringify({ port: 3000 }));
myVfs.writeFileSync('/app/greet.js', 'module.exports = (name) => "Hello, " + name + "!";');

// Mount the VFS at a path prefix
myVfs.mount('/virtual');

// Now standard fs operations work on the virtual files
const config = JSON.parse(fs.readFileSync('/virtual/app/config.json', 'utf8'));
console.log(config.port); // 3000

// Modules can be required from the VFS
const greet = await import('/virtual/app/greet.js');
console.log(greet.default('World')); // Hello, World!

// Clean up
myVfs.unmount();
```

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

// Create a new virtual file system
const myVfs = vfs.create();

// Create directories and files
myVfs.mkdirSync('/app');
myVfs.writeFileSync('/app/config.json', JSON.stringify({ port: 3000 }));
myVfs.writeFileSync('/app/greet.js', 'module.exports = (name) => "Hello, " + name + "!";');

// Mount the VFS at a path prefix
myVfs.mount('/virtual');

// Now standard fs operations work on the virtual files
const config = JSON.parse(fs.readFileSync('/virtual/app/config.json', 'utf8'));
console.log(config.port); // 3000

// Modules can be required from the VFS
const greet = require('/virtual/app/greet.js');
console.log(greet('World')); // Hello, World!

// Clean up
myVfs.unmount();
```

## `vfs.create([provider][, options])`

<!-- YAML
added: REPLACEME
-->

* `provider` {VirtualProvider} Optional provider instance. Defaults to a new
  `MemoryProvider`.
* `options` {Object}
  * `moduleHooks` {boolean} Whether to enable `require()`/`import` hooks for
    loading modules from the VFS. **Default:** `true`.
  * `virtualCwd` {boolean} Whether to enable virtual working directory support.
    **Default:** `false`.
  * `overlay` {boolean} Whether to enable overlay mode. In overlay mode, the VFS
    only intercepts paths that exist in the VFS, allowing other paths to fall
    through to the real file system. Useful for mocking specific files while
    leaving others unchanged. See [Security considerations][] for important
    warnings. **Default:** `false`.
* Returns: {VirtualFileSystem}

Creates a new `VirtualFileSystem` instance. If no provider is specified, a
`MemoryProvider` is used, which stores files in memory.

```mjs
import vfs from 'node:vfs';

// Create with default MemoryProvider
const memoryVfs = vfs.create();

// Create with explicit provider
const customVfs = vfs.create(new vfs.MemoryProvider());

// Create with options only
const vfsWithOptions = vfs.create({ moduleHooks: false });
```

```cjs
const vfs = require('node:vfs');

// Create with default MemoryProvider
const memoryVfs = vfs.create();

// Create with explicit provider
const customVfs = vfs.create(new vfs.MemoryProvider());

// Create with options only
const vfsWithOptions = vfs.create({ moduleHooks: false });
```

## Class: `VirtualFileSystem`

<!-- YAML
added: REPLACEME
-->

The `VirtualFileSystem` class provides a file system interface backed by a
provider. It supports standard file system operations and can be mounted to
make virtual files accessible through the `fs` module.

### `new VirtualFileSystem([provider][, options])`

<!-- YAML
added: REPLACEME
-->

* `provider` {VirtualProvider} The provider to use. **Default:** `MemoryProvider`.
* `options` {Object}
  * `moduleHooks` {boolean} Enable module loading hooks. **Default:** `true`.
  * `virtualCwd` {boolean} Enable virtual working directory. **Default:** `false`.

Creates a new `VirtualFileSystem` instance.

### `vfs.chdir(path)`

<!-- YAML
added: REPLACEME
-->

* `path` {string} The new working directory path within the VFS.

Changes the virtual working directory. This only affects path resolution within
the VFS when `virtualCwd` is enabled in the constructor options.

Throws `ERR_INVALID_STATE` if `virtualCwd` was not enabled during construction.

When mounted with `virtualCwd` enabled, the VFS also hooks `process.chdir()` and
`process.cwd()` to support virtual paths transparently. In Worker threads,
`process.chdir()` to virtual paths will work, but attempting to change to real
file system paths will throw `ERR_WORKER_UNSUPPORTED_OPERATION`.

### `vfs.cwd()`

<!-- YAML
added: REPLACEME
-->

* Returns: {string|null}

Returns the current virtual working directory, or `null` if no virtual directory
has been set yet.

Throws `ERR_INVALID_STATE` if `virtualCwd` was not enabled during construction.

### `vfs.mount(prefix)`

<!-- YAML
added: REPLACEME
-->

* `prefix` {string} The path prefix where the VFS will be mounted.
* Returns: {VirtualFileSystem} The VFS instance (for chaining or `using`).

Mounts the virtual file system at the specified path prefix. After mounting,
files in the VFS can be accessed via the `fs` module using paths that start
with the prefix.

If a real file system path already exists at the mount prefix, the VFS
**shadows** that path. All operations to paths under the mount prefix will be
directed to the VFS, making the real files inaccessible until the VFS is
unmounted. See [Security considerations][] for important warnings about this
behavior.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/data.txt', 'Hello');
myVfs.mount('/virtual');

// Now accessible as /virtual/data.txt
require('node:fs').readFileSync('/virtual/data.txt', 'utf8'); // 'Hello'
```

The VFS supports the [Explicit Resource Management][] proposal. Use the `using`
declaration to automatically unmount when leaving scope:

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

{
  using myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'Hello');
  myVfs.mount('/virtual');

  fs.readFileSync('/virtual/data.txt', 'utf8'); // 'Hello'
} // VFS is automatically unmounted here

fs.existsSync('/virtual/data.txt'); // false - VFS is unmounted
```

### `vfs.mounted`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Returns `true` if the VFS is currently mounted.

### `vfs.mountPoint`

<!-- YAML
added: REPLACEME
-->

* {string | null}

The current mount point, or `null` if not mounted.

### `vfs.overlay`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Returns `true` if overlay mode is enabled. In overlay mode, the VFS only
intercepts paths that exist in the VFS, allowing other paths to fall through
to the real file system.

### `vfs.provider`

<!-- YAML
added: REPLACEME
-->

* {VirtualProvider}

The underlying provider for this VFS instance. Can be used to access
provider-specific methods like `setReadOnly()` for `MemoryProvider`.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();

// Access the provider
console.log(myVfs.provider.readonly); // false
myVfs.provider.setReadOnly();
console.log(myVfs.provider.readonly); // true
```

### `vfs.readonly`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Returns `true` if the underlying provider is read-only.

### `vfs.unmount()`

<!-- YAML
added: REPLACEME
-->

Unmounts the virtual file system. After unmounting, virtual files are no longer
accessible through the `fs` module. The VFS can be remounted at the same or a
different path by calling `mount()` again. Unmounting also resets the virtual
working directory if one was set.

### File System Methods

The `VirtualFileSystem` class provides methods that mirror the `fs` module API.
All paths are relative to the VFS root (not the mount point).

These methods accept the same argument types as their `fs` counterparts,
including `string`, `Buffer`, `TypedArray`, and `DataView` where applicable.

#### Overlay mode behavior

When overlay mode is enabled, the following behavior applies to `fs` operations
on mounted paths:

* **Read operations** (`readFile`, `readdir`, `stat`, `lstat`, `access`,
  `exists`, `realpath`, `readlink`): Check VFS first. If the path doesn't exist
  in VFS, fall through to the real file system.
* **Write operations** (`writeFile`, `appendFile`, `mkdir`, `rename`, `unlink`,
  `rmdir`, `symlink`, `copyFile`): Always operate on VFS. New files are created
  in VFS, and attempting to modify a real file that doesn't exist in VFS will
  create a new VFS file instead.
* **File descriptors**: Once a file is opened, all subsequent operations on that
  descriptor stay within the same layer (VFS or real FS) where it was opened.

#### Synchronous Methods

* `vfs.accessSync(path[, mode])` - Check file accessibility
* `vfs.appendFileSync(path, data[, options])` - Append to a file
* `vfs.closeSync(fd)` - Close a file descriptor
* `vfs.copyFileSync(src, dest[, mode])` - Copy a file
* `vfs.existsSync(path)` - Check if path exists
* `vfs.lstatSync(path[, options])` - Get file stats (no symlink follow)
* `vfs.mkdirSync(path[, options])` - Create a directory
* `vfs.openSync(path, flags[, mode])` - Open a file
* `vfs.readdirSync(path[, options])` - Read directory contents
* `vfs.readFileSync(path[, options])` - Read a file
* `vfs.readlinkSync(path[, options])` - Read symlink target
* `vfs.readSync(fd, buffer, offset, length, position)` - Read from fd
* `vfs.realpathSync(path[, options])` - Resolve symlinks
* `vfs.renameSync(oldPath, newPath)` - Rename a file or directory
* `vfs.rmdirSync(path)` - Remove a directory
* `vfs.statSync(path[, options])` - Get file stats
* `vfs.symlinkSync(target, path[, type])` - Create a symlink
* `vfs.unlinkSync(path)` - Remove a file
* `vfs.writeFileSync(path, data[, options])` - Write a file
* `vfs.writeSync(fd, buffer, offset, length, position)` - Write to fd

#### Promise Methods

All synchronous methods have promise-based equivalents available through
`vfs.promises`:

```mjs
import vfs from 'node:vfs';

const myVfs = vfs.create();

await myVfs.promises.writeFile('/data.txt', 'Hello');
const content = await myVfs.promises.readFile('/data.txt', 'utf8');
console.log(content); // 'Hello'
```

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();

async function example() {
  await myVfs.promises.writeFile('/data.txt', 'Hello');
  const content = await myVfs.promises.readFile('/data.txt', 'utf8');
  console.log(content); // 'Hello'
}
```

## Class: `VirtualProvider`

<!-- YAML
added: REPLACEME
-->

The `VirtualProvider` class is an abstract base class for VFS providers.
Providers implement the actual file system storage and operations.

### Properties

#### `provider.readonly`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Returns `true` if the provider is read-only.

#### `provider.supportsSymlinks`

<!-- YAML
added: REPLACEME
-->

* {boolean}

Returns `true` if the provider supports symbolic links.

### Creating Custom Providers

To create a custom provider, extend `VirtualProvider` and implement the
required methods:

```cjs
const { VirtualProvider } = require('node:vfs');

class MyProvider extends VirtualProvider {
  get readonly() { return false; }
  get supportsSymlinks() { return true; }

  openSync(path, flags, mode) {
    // Implementation
  }

  statSync(path, options) {
    // Implementation
  }

  readdirSync(path, options) {
    // Implementation
  }

  // ... implement other required methods
}
```

## Class: `MemoryProvider`

<!-- YAML
added: REPLACEME
-->

The `MemoryProvider` stores files in memory. It supports full read/write
operations and symbolic links.

```cjs
const { create, MemoryProvider } = require('node:vfs');

const myVfs = create(new MemoryProvider());
```

### `memoryProvider.setReadOnly()`

<!-- YAML
added: REPLACEME
-->

Sets the provider to read-only mode. Once set to read-only, the provider
cannot be changed back to writable. This is useful for finalizing a VFS
after initial population.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();

// Populate the VFS
myVfs.mkdirSync('/app');
myVfs.writeFileSync('/app/config.json', '{"readonly": true}');

// Make it read-only
myVfs.provider.setReadOnly();

// This would now throw an error
// myVfs.writeFileSync('/app/config.json', 'new content');
```

## Class: `RealFSProvider`

<!-- YAML
added: REPLACEME
-->

The `RealFSProvider` wraps a real file system directory, allowing it to be
mounted at a different VFS path. This is useful for:

* Mounting a directory at a different path
* Enabling `virtualCwd` support in Worker threads (by mounting the real
  file system through VFS)
* Creating sandboxed views of real directories

### `new RealFSProvider(rootPath)`

<!-- YAML
added: REPLACEME
-->

* `rootPath` {string} The real file system path to use as the provider root.

Creates a new `RealFSProvider` that wraps the specified directory. All paths
accessed through this provider are resolved relative to `rootPath`. Path
traversal outside `rootPath` (via `..`) is prevented for security.

```mjs
import vfs from 'node:vfs';

// Mount /home/user/project at /project
const projectVfs = vfs.create(new vfs.RealFSProvider('/home/user/project'));
projectVfs.mount('/project');

// Now /project/src/index.js maps to /home/user/project/src/index.js
import fs from 'node:fs';
const content = fs.readFileSync('/project/src/index.js', 'utf8');
```

```cjs
const vfs = require('node:vfs');

// Mount /home/user/project at /project
const projectVfs = vfs.create(new vfs.RealFSProvider('/home/user/project'));
projectVfs.mount('/project');

// Now /project/src/index.js maps to /home/user/project/src/index.js
const fs = require('node:fs');
const content = fs.readFileSync('/project/src/index.js', 'utf8');
```

### Using `virtualCwd` in Worker threads

Since `process.chdir()` is not available in Worker threads, you can use
`RealFSProvider` to enable virtual working directory support:

```cjs
const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const vfs = require('node:vfs');

if (isMainThread) {
  new Worker(__filename);
} else {
  // In worker: mount real file system with virtualCwd enabled
  const realVfs = vfs.create(
    new vfs.RealFSProvider('/home/user/project'),
    { virtualCwd: true },
  );
  realVfs.mount('/project');

  // Now we can use virtual chdir in the worker
  realVfs.chdir('/project/src');
  console.log(realVfs.cwd()); // '/project/src'
}
```

### `realFSProvider.rootPath`

<!-- YAML
added: REPLACEME
-->

* {string}

The real file system path that this provider wraps.

## Integration with `fs` module

When a VFS is mounted, the standard `fs` module automatically routes operations
to the VFS for paths that match the mount prefix:

```mjs
import vfs from 'node:vfs';
import fs from 'node:fs';

const myVfs = vfs.create();
myVfs.writeFileSync('/hello.txt', 'Hello from VFS!');
myVfs.mount('/virtual');

// These all work transparently
fs.readFileSync('/virtual/hello.txt', 'utf8');        // Sync
await fs.promises.readFile('/virtual/hello.txt', 'utf8');   // Promise
fs.createReadStream('/virtual/hello.txt');            // Stream

// Real file system is still accessible
fs.readFileSync('/etc/passwd');  // Real file
```

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

const myVfs = vfs.create();
myVfs.writeFileSync('/hello.txt', 'Hello from VFS!');
myVfs.mount('/virtual');

// These all work transparently
fs.readFileSync('/virtual/hello.txt', 'utf8');        // Sync
fs.promises.readFile('/virtual/hello.txt', 'utf8');   // Promise
fs.createReadStream('/virtual/hello.txt');            // Stream

// Real file system is still accessible
fs.readFileSync('/etc/passwd');  // Real file
```

## Integration with module loading

Virtual files can be loaded as modules using `require()` or `import`:

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/math.js', `
  exports.add = (a, b) => a + b;
  exports.multiply = (a, b) => a * b;
`);
myVfs.mount('/modules');

const math = require('/modules/math.js');
console.log(math.add(2, 3)); // 5
```

```mjs
import vfs from 'node:vfs';

const myVfs = vfs.create();
myVfs.writeFileSync('/greet.mjs', `
  export default function greet(name) {
    return \`Hello, \${name}!\`;
  }
`);
myVfs.mount('/modules');

const { default: greet } = await import('/modules/greet.mjs');
console.log(greet('World')); // Hello, World!
```

## Implementation details

### Stats objects

The VFS returns real `fs.Stats` objects from `stat()`, `lstat()`, and `fstat()`
operations. These Stats objects behave identically to those returned by the real
file system:

* `stats.isFile()`, `stats.isDirectory()`, `stats.isSymbolicLink()` work correctly
* `stats.size` reflects the actual content size
* `stats.mtime`, `stats.ctime`, `stats.birthtime` are tracked per file
* `stats.mode` includes the file type bits and permissions

### File descriptors

Virtual file descriptors start at 10000 to avoid conflicts with real operating
system file descriptors. This allows the VFS to coexist with real file system
operations without file descriptor collisions.

## Use with Single Executable Applications

When running as a Single Executable Application (SEA), bundled assets are
automatically mounted at `/sea`. No additional setup is required:

```cjs
// In your SEA entry script
const fs = require('node:fs');

// Access bundled assets directly - they are automatically available at /sea
const config = JSON.parse(fs.readFileSync('/sea/config.json', 'utf8'));
const template = fs.readFileSync('/sea/templates/index.html', 'utf8');
```

See the [Single Executable Applications][] documentation for more information
on creating SEA builds with assets.

## Symbolic links

The VFS supports symbolic links within the virtual file system. Symlinks are
created using `vfs.symlinkSync()` or `vfs.promises.symlink()` and can point
to files or directories within the same VFS.

### Cross-boundary symlinks

Symbolic links in the VFS are **VFS-internal only**. They cannot:

* Point from a VFS path to a real file system path
* Point from a real file system path to a VFS path
* Be followed across VFS mount boundaries

When resolving symlinks, the VFS only follows links that target paths within
the same VFS instance. Attempts to create symlinks with absolute paths that
would resolve outside the VFS are allowed but will result in dangling symlinks.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/data');
myVfs.writeFileSync('/data/config.json', '{}');

// This works - symlink within VFS
myVfs.symlinkSync('/data/config.json', '/config');
myVfs.readFileSync('/config', 'utf8'); // '{}'

// This creates a dangling symlink - target doesn't exist in VFS
myVfs.symlinkSync('/etc/passwd', '/passwd-link');
// myVfs.readFileSync('/passwd-link'); // Throws ENOENT
```

### Symlinks in overlay mode

In overlay mode (`{ overlay: true }`), VFS and real file system symlinks remain
completely independent:

* **VFS symlinks** can only target other VFS paths. A VFS symlink cannot point
  to a real file system file, even if that file exists at the same logical path.
* **Real file system symlinks** can only target other real file system paths.
  A real symlink cannot point to a VFS file.
* **No cross-layer resolution** occurs. When following a symlink, the resolution
  stays entirely within either the VFS layer or the real file system layer.

```cjs
const vfs = require('node:vfs');
const fs = require('node:fs');

const myVfs = vfs.create({ overlay: true });
myVfs.mkdirSync('/data');
myVfs.writeFileSync('/data/config.json', '{"source": "vfs"}');
myVfs.symlinkSync('/data/config.json', '/data/link');
myVfs.mount('/app');

// VFS symlink resolves within VFS
fs.readFileSync('/app/data/link', 'utf8'); // '{"source": "vfs"}'

// If /app/data/real-link is a real FS symlink pointing to /app/data/config.json,
// it will NOT resolve to the VFS file - it looks for a real file at that path
```

This design ensures predictable behavior: symlinks always resolve within their
own layer, preventing unexpected interactions between virtual and real files.

## Worker threads

VFS instances are **not shared across worker threads**. Each worker thread has
its own V8 isolate and module cache, which means:

* A VFS mounted in the main thread is not accessible from worker threads
* Each worker thread must create and mount its own VFS instance
* VFS data is not synchronized between threads - changes in one thread are not
  visible in another

If you need to share virtual file content with worker threads, you must either:

1. **Recreate the VFS in each worker** - Pass the data to workers via
   `workerData` and have each worker create its own VFS:

```cjs
const { Worker, isMainThread, workerData } = require('node:worker_threads');
const vfs = require('node:vfs');

if (isMainThread) {
  const fileData = { '/config.json': '{"key": "value"}' };
  new Worker(__filename, { workerData: fileData });
} else {
  // Worker: recreate VFS from passed data
  const myVfs = vfs.create();
  for (const [path, content] of Object.entries(workerData)) {
    myVfs.writeFileSync(path, content);
  }
  myVfs.mount('/virtual');
  // Now the worker has its own copy of the VFS
}
```

2. **Use `RealFSProvider`** - If the data exists on the real file system, use
   `RealFSProvider` in each worker to mount the same directory.

This limitation exists because implementing cross-thread VFS access would
require moving the implementation to C++ with shared memory management, which
significantly increases complexity. This may be addressed in future versions.

## Security considerations

### Path shadowing

When a VFS is mounted, it **shadows** any real file system paths under the
mount prefix. This means:

* Real files at the mount path become inaccessible
* All operations are redirected to the VFS
* Modules loaded from shadowed paths will use VFS content

This behavior can be exploited maliciously. A module could mount a VFS over
critical system paths (like `/etc` on Unix or `C:\Windows` on Windows) and
intercept sensitive operations:

```cjs
// WARNING: Example of dangerous behavior - DO NOT DO THIS
const vfs = require('node:vfs');

const maliciousVfs = vfs.create();
maliciousVfs.writeFileSync('/passwd', 'malicious content');
maliciousVfs.mount('/etc');  // Shadows /etc/passwd!

// Now fs.readFileSync('/etc/passwd') returns 'malicious content'
```

### Overlay mode risks

Overlay mode (`{ overlay: true }`) allows a VFS to selectively intercept file
operations only for paths that exist in the VFS. While this is useful for
mocking specific files in tests, it can also be exploited to covertly intercept
access to specific files:

```cjs
// WARNING: Example of dangerous behavior - DO NOT DO THIS
const vfs = require('node:vfs');

// Create an overlay VFS that intercepts a specific file
const spyVfs = vfs.create(new vfs.MemoryProvider(), { overlay: true });
spyVfs.writeFileSync('/etc/shadow', 'intercepted!');
spyVfs.mount('/');  // Mount at root with overlay mode

// Only /etc/shadow is intercepted, other files work normally
fs.readFileSync('/etc/passwd');  // Real file (works normally)
fs.readFileSync('/etc/shadow');  // Returns 'intercepted!' (mocked)
```

This is particularly dangerous because:

* It's harder to detect than full path shadowing
* Only specific targeted files are affected
* Other operations appear to work normally

### Recommendations

* **Audit dependencies**: Be cautious of third-party modules that use VFS, as
  they could shadow important paths.
* **Use unique mount points**: Mount VFS at paths that don't conflict with
  real file system paths, such as `/@virtual` or `/vfs-{unique-id}`.
* **Verify mount points**: Before trusting file content from paths that could
  be shadowed, verify the mount state.
* **Limit VFS usage**: Only use VFS in controlled environments where you trust
  all loaded modules.

[Explicit Resource Management]: https://github.com/tc39/proposal-explicit-resource-management
[Security considerations]: #security-considerations
[Single Executable Applications]: single-executable-applications.md
