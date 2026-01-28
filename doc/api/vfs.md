# Virtual File System

<!--introduced_in=v26.0.0-->

<!-- YAML
added: v26.0.0
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
added: v26.0.0
-->

* `provider` {VirtualProvider} Optional provider instance. Defaults to a new
  `MemoryProvider`.
* `options` {Object}
  * `moduleHooks` {boolean} Whether to enable `require()`/`import` hooks for
    loading modules from the VFS. **Default:** `true`.
  * `virtualCwd` {boolean} Whether to enable virtual working directory support.
    **Default:** `false`.
* Returns: {VirtualFileSystem}

Creates a new `VirtualFileSystem` instance. If no provider is specified, a
`MemoryProvider` is used, which stores files in memory.

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
added: v26.0.0
-->

The `VirtualFileSystem` class provides a file system interface backed by a
provider. It supports standard file system operations and can be mounted to
make virtual files accessible through the `fs` module.

### `new VirtualFileSystem([provider][, options])`

<!-- YAML
added: v26.0.0
-->

* `provider` {VirtualProvider} The provider to use. **Default:** `MemoryProvider`.
* `options` {Object}
  * `moduleHooks` {boolean} Enable module loading hooks. **Default:** `true`.
  * `virtualCwd` {boolean} Enable virtual working directory. **Default:** `false`.

Creates a new `VirtualFileSystem` instance.

### `vfs.provider`

<!-- YAML
added: v26.0.0
-->

* {VirtualProvider}

The underlying provider for this VFS instance. Can be used to access
provider-specific methods like `setContentProvider()` and `setPopulateCallback()`
for `MemoryProvider`.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();

// Access the provider for advanced features
myVfs.provider.setContentProvider('/dynamic.txt', () => {
  return `Time: ${Date.now()}`;
});
```

### `vfs.mount(prefix)`

<!-- YAML
added: v26.0.0
-->

* `prefix` {string} The path prefix where the VFS will be mounted.

Mounts the virtual file system at the specified path prefix. After mounting,
files in the VFS can be accessed via the `fs` module using paths that start
with the prefix.

```cjs
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/data.txt', 'Hello');
myVfs.mount('/virtual');

// Now accessible as /virtual/data.txt
require('node:fs').readFileSync('/virtual/data.txt', 'utf8'); // 'Hello'
```

### `vfs.unmount()`

<!-- YAML
added: v26.0.0
-->

Unmounts the virtual file system. After unmounting, virtual files are no longer
accessible through the `fs` module.

### `vfs.isMounted`

<!-- YAML
added: v26.0.0
-->

* {boolean}

Returns `true` if the VFS is currently mounted.

### `vfs.mountPoint`

<!-- YAML
added: v26.0.0
-->

* {string | null}

The current mount point, or `null` if not mounted.

### `vfs.readonly`

<!-- YAML
added: v26.0.0
-->

* {boolean}

Returns `true` if the underlying provider is read-only.

### `vfs.chdir(path)`

<!-- YAML
added: v26.0.0
-->

* `path` {string} The new working directory path within the VFS.

Changes the virtual working directory. This only affects path resolution within
the VFS when `virtualCwd` is enabled.

### `vfs.cwd()`

<!-- YAML
added: v26.0.0
-->

* Returns: {string}

Returns the current virtual working directory.

### File System Methods

The `VirtualFileSystem` class provides methods that mirror the `fs` module API.
All paths are relative to the VFS root (not the mount point).

#### Synchronous Methods

* `vfs.readFileSync(path[, options])` - Read a file
* `vfs.writeFileSync(path, data[, options])` - Write a file
* `vfs.appendFileSync(path, data[, options])` - Append to a file
* `vfs.statSync(path[, options])` - Get file stats
* `vfs.lstatSync(path[, options])` - Get file stats (no symlink follow)
* `vfs.readdirSync(path[, options])` - Read directory contents
* `vfs.mkdirSync(path[, options])` - Create a directory
* `vfs.rmdirSync(path)` - Remove a directory
* `vfs.unlinkSync(path)` - Remove a file
* `vfs.renameSync(oldPath, newPath)` - Rename a file or directory
* `vfs.copyFileSync(src, dest[, mode])` - Copy a file
* `vfs.existsSync(path)` - Check if path exists
* `vfs.accessSync(path[, mode])` - Check file accessibility
* `vfs.openSync(path, flags[, mode])` - Open a file
* `vfs.closeSync(fd)` - Close a file descriptor
* `vfs.readSync(fd, buffer, offset, length, position)` - Read from fd
* `vfs.writeSync(fd, buffer, offset, length, position)` - Write to fd
* `vfs.realpathSync(path[, options])` - Resolve symlinks
* `vfs.readlinkSync(path[, options])` - Read symlink target
* `vfs.symlinkSync(target, path[, type])` - Create a symlink

#### Promise Methods

All synchronous methods have promise-based equivalents available through
`vfs.promises`:

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
added: v26.0.0
-->

The `VirtualProvider` class is an abstract base class for VFS providers.
Providers implement the actual file system storage and operations.

### Properties

#### `provider.readonly`

<!-- YAML
added: v26.0.0
-->

* {boolean}

Returns `true` if the provider is read-only.

#### `provider.supportsSymlinks`

<!-- YAML
added: v26.0.0
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
added: v26.0.0
-->

The `MemoryProvider` stores files in memory. It supports full read/write
operations and symbolic links.

```cjs
const { create, MemoryProvider } = require('node:vfs');

const myVfs = create(new MemoryProvider());
```

### `memoryProvider.setReadOnly()`

<!-- YAML
added: v26.0.0
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

## Class: `SEAProvider`

<!-- YAML
added: v26.0.0
-->

The `SEAProvider` provides read-only access to assets bundled in a Single
Executable Application (SEA). It can only be used when running as a SEA.

```cjs
const { create, SEAProvider } = require('node:vfs');

// Only works in SEA builds
try {
  const seaVfs = create(new SEAProvider());
  seaVfs.mount('/assets');
} catch (err) {
  console.log('Not running as SEA');
}
```

## Integration with `fs` module

When a VFS is mounted, the standard `fs` module automatically routes operations
to the VFS for paths that match the mount prefix:

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

[Single Executable Applications]: single-executable-applications.md
