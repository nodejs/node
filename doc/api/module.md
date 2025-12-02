# Modules: `node:module` API

<!--introduced_in=v12.20.0-->

<!-- YAML
added: v0.3.7
-->

## The `Module` object

* Type: {Object}

Provides general utility methods when interacting with instances of
`Module`, the [`module`][] variable often seen in [CommonJS][] modules. Accessed
via `import 'node:module'` or `require('node:module')`.

### `module.builtinModules`

<!-- YAML
added:
  - v9.3.0
  - v8.10.0
  - v6.13.0
changes:
  - version: v23.5.0
    pr-url: https://github.com/nodejs/node/pull/56185
    description: The list now also contains prefix-only modules.
-->

* Type: {string\[]}

A list of the names of all modules provided by Node.js. Can be used to verify
if a module is maintained by a third party or not.

`module` in this context isn't the same object that's provided
by the [module wrapper][]. To access it, require the `Module` module:

```mjs
// module.mjs
// In an ECMAScript module
import { builtinModules as builtin } from 'node:module';
```

```cjs
// module.cjs
// In a CommonJS module
const builtin = require('node:module').builtinModules;
```

### `module.createRequire(filename)`

<!-- YAML
added: v12.2.0
-->

* `filename` {string|URL} Filename to be used to construct the require
  function. Must be a file URL object, file URL string, or absolute path
  string.
* Returns: {require} Require function

```mjs
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

// sibling-module.js is a CommonJS module.
const siblingModule = require('./sibling-module');
```

### `module.findPackageJSON(specifier[, base])`

<!-- YAML
added:
  - v23.2.0
  - v22.14.0
-->

> Stability: 1.1 - Active Development

* `specifier` {string|URL} The specifier for the module whose `package.json` to
  retrieve. When passing a _bare specifier_, the `package.json` at the root of
  the package is returned. When passing a _relative specifier_ or an _absolute specifier_,
  the closest parent `package.json` is returned.
* `base` {string|URL} The absolute location (`file:` URL string or FS path) of the
  containing  module. For CJS, use `__filename` (not `__dirname`!); for ESM, use
  `import.meta.url`. You do not need to pass it if `specifier` is an `absolute specifier`.
* Returns: {string|undefined} A path if the `package.json` is found. When `specifier`
  is a package, the package's root `package.json`; when a relative or unresolved, the closest
  `package.json` to the `specifier`.

> **Caveat**: Do not use this to try to determine module format. There are many things affecting
> that determination; the `type` field of package.json is the _least_ definitive (ex file extension
> supersedes it, and a loader hook supersedes that).

> **Caveat**: This currently leverages only the built-in default resolver; if
> [`resolve` customization hooks][resolve hook] are registered, they will not affect the resolution.
> This may change in the future.

```text
/path/to/project
  ├ packages/
    ├ bar/
      ├ bar.js
      └ package.json // name = '@foo/bar'
    └ qux/
      ├ node_modules/
        └ some-package/
          └ package.json // name = 'some-package'
      ├ qux.js
      └ package.json // name = '@foo/qux'
  ├ main.js
  └ package.json // name = '@foo'
```

```mjs
// /path/to/project/packages/bar/bar.js
import { findPackageJSON } from 'node:module';

findPackageJSON('..', import.meta.url);
// '/path/to/project/package.json'
// Same result when passing an absolute specifier instead:
findPackageJSON(new URL('../', import.meta.url));
findPackageJSON(import.meta.resolve('../'));

findPackageJSON('some-package', import.meta.url);
// '/path/to/project/packages/bar/node_modules/some-package/package.json'
// When passing an absolute specifier, you might get a different result if the
// resolved module is inside a subfolder that has nested `package.json`.
findPackageJSON(import.meta.resolve('some-package'));
// '/path/to/project/packages/bar/node_modules/some-package/some-subfolder/package.json'

findPackageJSON('@foo/qux', import.meta.url);
// '/path/to/project/packages/qux/package.json'
```

```cjs
// /path/to/project/packages/bar/bar.js
const { findPackageJSON } = require('node:module');
const { pathToFileURL } = require('node:url');
const path = require('node:path');

findPackageJSON('..', __filename);
// '/path/to/project/package.json'
// Same result when passing an absolute specifier instead:
findPackageJSON(pathToFileURL(path.join(__dirname, '..')));

findPackageJSON('some-package', __filename);
// '/path/to/project/packages/bar/node_modules/some-package/package.json'
// When passing an absolute specifier, you might get a different result if the
// resolved module is inside a subfolder that has nested `package.json`.
findPackageJSON(pathToFileURL(require.resolve('some-package')));
// '/path/to/project/packages/bar/node_modules/some-package/some-subfolder/package.json'

findPackageJSON('@foo/qux', __filename);
// '/path/to/project/packages/qux/package.json'
```

### `module.isBuiltin(moduleName)`

<!-- YAML
added:
  - v18.6.0
  - v16.17.0
-->

* `moduleName` {string} name of the module
* Returns: {boolean} returns true if the module is builtin else returns false

```mjs
import { isBuiltin } from 'node:module';
isBuiltin('node:fs'); // true
isBuiltin('fs'); // true
isBuiltin('wss'); // false
```

### `module.register(specifier[, parentURL][, options])`

<!-- YAML
added:
  - v20.6.0
  - v18.19.0
changes:
  - version:
    - v23.6.1
    - v22.13.1
    - v20.18.2
    pr-url: https://github.com/nodejs-private/node-private/pull/629
    description: Using this feature with the permission model enabled requires
                 passing `--allow-worker`.
  - version:
    - v20.8.0
    - v18.19.0
    pr-url: https://github.com/nodejs/node/pull/49655
    description: Add support for WHATWG URL instances.
-->

> Stability: 1.1 - Active development

* `specifier` {string|URL} Customization hooks to be registered; this should be
  the same string that would be passed to `import()`, except that if it is
  relative, it is resolved relative to `parentURL`.
* `parentURL` {string|URL} If you want to resolve `specifier` relative to a base
  URL, such as `import.meta.url`, you can pass that URL here. **Default:**
  `'data:'`
* `options` {Object}
  * `parentURL` {string|URL} If you want to resolve `specifier` relative to a
    base URL, such as `import.meta.url`, you can pass that URL here. This
    property is ignored if the `parentURL` is supplied as the second argument.
    **Default:** `'data:'`
  * `data` {any} Any arbitrary, cloneable JavaScript value to pass into the
    [`initialize`][] hook.
  * `transferList` {Object\[]} [transferable objects][] to be passed into the
    `initialize` hook.

Register a module that exports [hooks][] that customize Node.js module
resolution and loading behavior. See [Customization hooks][].

This feature requires `--allow-worker` if used with the [Permission Model][].

### `module.registerHooks(options)`

<!-- YAML
added:
  - v23.5.0
  - v22.15.0
changes:
  - version:
    - REPLACEME
    pr-url: https://github.com/nodejs/node/pull/60960
    description: Synchronous and in-thread hooks are now release candidate.
-->

> Stability: 1.1 - Active development

* `options` {Object}
  * `load` {Function|undefined} See [load hook][]. **Default:** `undefined`.
  * `resolve` {Function|undefined} See [resolve hook][]. **Default:** `undefined`.

Register [hooks][] that customize Node.js module resolution and loading behavior.
See [Customization hooks][].

### `module.stripTypeScriptTypes(code[, options])`

<!-- YAML
added:
  - v23.2.0
  - v22.13.0
-->

> Stability: 1.2 - Release candidate

* `code` {string} The code to strip type annotations from.
* `options` {Object}
  * `mode` {string} **Default:** `'strip'`. Possible values are:
    * `'strip'` Only strip type annotations without performing the transformation of TypeScript features.
    * `'transform'` Strip type annotations and transform TypeScript features to JavaScript.
  * `sourceMap` {boolean} **Default:** `false`. Only when `mode` is `'transform'`, if `true`, a source map
    will be generated for the transformed code.
  * `sourceUrl` {string}  Specifies the source url used in the source map.
* Returns: {string} The code with type annotations stripped.
  `module.stripTypeScriptTypes()` removes type annotations from TypeScript code. It
  can be used to strip type annotations from TypeScript code before running it
  with `vm.runInContext()` or `vm.compileFunction()`.
  By default, it will throw an error if the code contains TypeScript features
  that require transformation such as `Enums`,
  see [type-stripping][] for more information.
  When mode is `'transform'`, it also transforms TypeScript features to JavaScript,
  see [transform TypeScript features][] for more information.
  When mode is `'strip'`, source maps are not generated, because locations are preserved.
  If `sourceMap` is provided, when mode is `'strip'`, an error will be thrown.

_WARNING_: The output of this function should not be considered stable across Node.js versions,
due to changes in the TypeScript parser.

```mjs
import { stripTypeScriptTypes } from 'node:module';
const code = 'const a: number = 1;';
const strippedCode = stripTypeScriptTypes(code);
console.log(strippedCode);
// Prints: const a         = 1;
```

```cjs
const { stripTypeScriptTypes } = require('node:module');
const code = 'const a: number = 1;';
const strippedCode = stripTypeScriptTypes(code);
console.log(strippedCode);
// Prints: const a         = 1;
```

If `sourceUrl` is provided, it will be used appended as a comment at the end of the output:

```mjs
import { stripTypeScriptTypes } from 'node:module';
const code = 'const a: number = 1;';
const strippedCode = stripTypeScriptTypes(code, { mode: 'strip', sourceUrl: 'source.ts' });
console.log(strippedCode);
// Prints: const a         = 1\n\n//# sourceURL=source.ts;
```

```cjs
const { stripTypeScriptTypes } = require('node:module');
const code = 'const a: number = 1;';
const strippedCode = stripTypeScriptTypes(code, { mode: 'strip', sourceUrl: 'source.ts' });
console.log(strippedCode);
// Prints: const a         = 1\n\n//# sourceURL=source.ts;
```

When `mode` is `'transform'`, the code is transformed to JavaScript:

```mjs
import { stripTypeScriptTypes } from 'node:module';
const code = `
  namespace MathUtil {
    export const add = (a: number, b: number) => a + b;
  }`;
const strippedCode = stripTypeScriptTypes(code, { mode: 'transform', sourceMap: true });
console.log(strippedCode);
// Prints:
// var MathUtil;
// (function(MathUtil) {
//     MathUtil.add = (a, b)=>a + b;
// })(MathUtil || (MathUtil = {}));
// # sourceMappingURL=data:application/json;base64, ...
```

```cjs
const { stripTypeScriptTypes } = require('node:module');
const code = `
  namespace MathUtil {
    export const add = (a: number, b: number) => a + b;
  }`;
const strippedCode = stripTypeScriptTypes(code, { mode: 'transform', sourceMap: true });
console.log(strippedCode);
// Prints:
// var MathUtil;
// (function(MathUtil) {
//     MathUtil.add = (a, b)=>a + b;
// })(MathUtil || (MathUtil = {}));
// # sourceMappingURL=data:application/json;base64, ...
```

### `module.syncBuiltinESMExports()`

<!-- YAML
added: v12.12.0
-->

The `module.syncBuiltinESMExports()` method updates all the live bindings for
builtin [ES Modules][] to match the properties of the [CommonJS][] exports. It
does not add or remove exported names from the [ES Modules][].

```js
const fs = require('node:fs');
const assert = require('node:assert');
const { syncBuiltinESMExports } = require('node:module');

fs.readFile = newAPI;

delete fs.readFileSync;

function newAPI() {
  // ...
}

fs.newAPI = newAPI;

syncBuiltinESMExports();

import('node:fs').then((esmFS) => {
  // It syncs the existing readFile property with the new value
  assert.strictEqual(esmFS.readFile, newAPI);
  // readFileSync has been deleted from the required fs
  assert.strictEqual('readFileSync' in fs, false);
  // syncBuiltinESMExports() does not remove readFileSync from esmFS
  assert.strictEqual('readFileSync' in esmFS, true);
  // syncBuiltinESMExports() does not add names
  assert.strictEqual(esmFS.newAPI, undefined);
});
```

## Module compile cache

<!-- YAML
added: v22.1.0
changes:
  - version: v22.8.0
    pr-url: https://github.com/nodejs/node/pull/54501
    description: add initial JavaScript APIs for runtime access.
-->

The module compile cache can be enabled either using the [`module.enableCompileCache()`][]
method or the [`NODE_COMPILE_CACHE=dir`][] environment variable. After it is enabled,
whenever Node.js compiles a CommonJS, a ECMAScript Module, or a TypeScript module, it will
use on-disk [V8 code cache][] persisted in the specified directory to speed up the compilation.
This may slow down the first load of a module graph, but subsequent loads of the same module
graph may get a significant speedup if the contents of the modules do not change.

To clean up the generated compile cache on disk, simply remove the cache directory. The cache
directory will be recreated the next time the same directory is used for for compile cache
storage. To avoid filling up the disk with stale cache, it is recommended to use a directory
under the [`os.tmpdir()`][]. If the compile cache is enabled by a call to
[`module.enableCompileCache()`][] without specifying the `directory`, Node.js will use
the [`NODE_COMPILE_CACHE=dir`][] environment variable if it's set, or defaults
to `path.join(os.tmpdir(), 'node-compile-cache')` otherwise. To locate the compile cache
directory used by a running Node.js instance, use [`module.getCompileCacheDir()`][].

The enabled module compile cache can be disabled by the [`NODE_DISABLE_COMPILE_CACHE=1`][]
environment variable. This can be useful when the compile cache leads to unexpected or
undesired behaviors (e.g. less precise test coverage).

At the moment, when the compile cache is enabled and a module is loaded afresh, the
code cache is generated from the compiled code immediately, but will only be written
to disk when the Node.js instance is about to exit. This is subject to change. The
[`module.flushCompileCache()`][] method can be used to ensure the accumulated code cache
is flushed to disk in case the application wants to spawn other Node.js instances
and let them share the cache long before the parent exits.

The compile cache layout on disk is an implementation detail and should not be
relied upon. The compile cache generated is typically only reusable in the same
version of Node.js, and should be not assumed to be compatible across different
versions of Node.js.

### Portability of the compile cache

By default, caches are invalidated when the absolute paths of the modules being
cached are changed. To keep the cache working after moving the
project directory, enable portable compile cache. This allows previously compiled
modules to be reused across different directory locations as long as the layout relative
to the cache directory remains the same. This would be done on a best-effort basis. If
Node.js cannot compute the location of a module relative to the cache directory, the module
will not be cached.

There are two ways to enable the portable mode:

1. Using the portable option in [`module.enableCompileCache()`][]:

   ```js
   // Non-portable cache (default): cache breaks if project is moved
   module.enableCompileCache({ directory: '/path/to/cache/storage/dir' });

   // Portable cache: cache works after the project is moved
   module.enableCompileCache({ directory: '/path/to/cache/storage/dir', portable: true });
   ```

2. Setting the environment variable: [`NODE_COMPILE_CACHE_PORTABLE=1`][]

### Limitations of the compile cache

Currently when using the compile cache with [V8 JavaScript code coverage][], the
coverage being collected by V8 may be less precise in functions that are
deserialized from the code cache. It's recommended to turn this off when
running tests to generate precise coverage.

Compilation cache generated by one version of Node.js can not be reused by a different
version of Node.js. Cache generated by different versions of Node.js will be stored
separately if the same base directory is used to persist the cache, so they can co-exist.

### `module.constants.compileCacheStatus`

<!-- YAML
added: v22.8.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/60971
    description: This feature is no longer experimental.
-->

The following constants are returned as the `status` field in the object returned by
[`module.enableCompileCache()`][] to indicate the result of the attempt to enable the
[module compile cache][].

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>ENABLED</code></td>
    <td>
      Node.js has enabled the compile cache successfully. The directory used to store the
      compile cache will be returned in the <code>directory</code> field in the
      returned object.
    </td>
  </tr>
  <tr>
    <td><code>ALREADY_ENABLED</code></td>
    <td>
      The compile cache has already been enabled before, either by a previous call to
      <code>module.enableCompileCache()</code>, or by the <code>NODE_COMPILE_CACHE=dir</code>
      environment variable. The directory used to store the
      compile cache will be returned in the <code>directory</code> field in the
      returned object.
    </td>
  </tr>
  <tr>
    <td><code>FAILED</code></td>
    <td>
      Node.js fails to enable the compile cache. This can be caused by the lack of
      permission to use the specified directory, or various kinds of file system errors.
      The detail of the failure will be returned in the <code>message</code> field in the
      returned object.
    </td>
  </tr>
  <tr>
    <td><code>DISABLED</code></td>
    <td>
      Node.js cannot enable the compile cache because the environment variable
      <code>NODE_DISABLE_COMPILE_CACHE=1</code> has been set.
    </td>
  </tr>
</table>

### `module.enableCompileCache([options])`

<!-- YAML
added: v22.8.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/60971
    description: This feature is no longer experimental.
  - version: v25.0.0
    pr-url: https://github.com/nodejs/node/pull/58797
    description: Add `portable` option to enable portable compile cache.
  - version: v25.0.0
    pr-url: https://github.com/nodejs/node/pull/59931
    description: Rename the unreleased `path` option to `directory` to maintain consistency.
-->

* `options` {string|Object} Optional. If a string is passed, it is considered to be `options.directory`.
  * `directory` {string} Optional. Directory to store the compile cache. If not specified,
    the directory specified by the [`NODE_COMPILE_CACHE=dir`][] environment variable
    will be used if it's set, or `path.join(os.tmpdir(), 'node-compile-cache')`
    otherwise.
  * `portable` {boolean} Optional. If `true`, enables portable compile cache so that
    the cache can be reused even if the project directory is moved. This is a best-effort
    feature. If not specified, it will depend on whether the environment variable
    [`NODE_COMPILE_CACHE_PORTABLE=1`][] is set.
* Returns: {Object}
  * `status` {integer} One of the [`module.constants.compileCacheStatus`][]
  * `message` {string|undefined} If Node.js cannot enable the compile cache, this contains
    the error message. Only set if `status` is `module.constants.compileCacheStatus.FAILED`.
  * `directory` {string|undefined} If the compile cache is enabled, this contains the directory
    where the compile cache is stored. Only set if  `status` is
    `module.constants.compileCacheStatus.ENABLED` or
    `module.constants.compileCacheStatus.ALREADY_ENABLED`.

Enable [module compile cache][] in the current Node.js instance.

For general use cases, it's recommended to call `module.enableCompileCache()` without
specifying the `options.directory`, so that the directory can be overridden by the
`NODE_COMPILE_CACHE` environment variable when necessary.

Since compile cache is supposed to be a optimization that is not mission critical, this
method is designed to not throw any exception when the compile cache cannot be enabled.
Instead, it will return an object containing an error message in the `message` field to
aid debugging. If compile cache is enabled successfully, the `directory` field in the
returned object contains the path to the directory where the compile cache is stored. The
`status` field in the returned object would be one of the `module.constants.compileCacheStatus`
values to indicate the result of the attempt to enable the [module compile cache][].

This method only affects the current Node.js instance. To enable it in child worker threads,
either call this method in child worker threads too, or set the
`process.env.NODE_COMPILE_CACHE` value to compile cache directory so the behavior can
be inherited into the child workers. The directory can be obtained either from the
`directory` field returned by this method, or with [`module.getCompileCacheDir()`][].

### `module.flushCompileCache()`

<!-- YAML
added:
 - v23.0.0
 - v22.10.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/60971
    description: This feature is no longer experimental.
-->

Flush the [module compile cache][] accumulated from modules already loaded
in the current Node.js instance to disk. This returns after all the flushing
file system operations come to an end, no matter they succeed or not. If there
are any errors, this will fail silently, since compile cache misses should not
interfere with the actual operation of the application.

### `module.getCompileCacheDir()`

<!-- YAML
added: v22.8.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/60971
    description: This feature is no longer experimental.
-->

* Returns: {string|undefined} Path to the [module compile cache][] directory if it is enabled,
  or `undefined` otherwise.

<i id="module_customization_hooks"></i>

## Customization Hooks

<!-- YAML
added: v8.8.0
changes:
  - version:
    - v23.5.0
    - v22.15.0
    pr-url: https://github.com/nodejs/node/pull/55698
    description: Add support for synchronous and in-thread hooks.
  - version:
    - v20.6.0
    - v18.19.0
    pr-url: https://github.com/nodejs/node/pull/48842
    description: Added `initialize` hook to replace `globalPreload`.
  - version:
    - v18.6.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42623
    description: Add support for chaining loaders.
  - version: v16.12.0
    pr-url: https://github.com/nodejs/node/pull/37468
    description: Removed `getFormat`, `getSource`, `transformSource`, and
                 `globalPreload`; added `load` hook and `getGlobalPreload` hook.
-->

<!-- type=misc -->

Node.js currently supports two types of module customization hooks:

1. [`module.registerHooks(options)`][`module.registerHooks()`]: takes synchronous hook
   functions that are run directly on the thread where the modules are loaded.
2. [`module.register(specifier[, parentURL][, options])`][`register`]: takes specifier to a
   module that exports asynchronous hook functions. The functions are run on a
   separate loader thread.

The asynchronous hooks incur extra overhead from inter-thread communication,
and have [several caveats][caveats of asynchronous customization hooks] especially
when customizing CommonJS modules in the module graph.
In most cases, it's recommended to use synchronous hooks via `module.registerHooks()`
for simplicity.

### Synchronous customization hooks

> Stability: 1.1 - Active Development

<i id="enabling_module_customization_hooks"></i>

#### Registration of synchronous customization hooks

To register synchronous customization hooks, use [`module.registerHooks()`][], which
takes [synchronous hook functions][] directly in-line.

```mjs
// register-hooks.js
import { registerHooks } from 'node:module';
registerHooks({
  resolve(specifier, context, nextResolve) { /* implementation */ },
  load(url, context, nextLoad) { /* implementation */ },
});
```

```cjs
// register-hooks.js
const { registerHooks } = require('node:module');
registerHooks({
  resolve(specifier, context, nextResolve) { /* implementation */ },
  load(url, context, nextLoad) { /* implementation */ },
});
```

##### Registering hooks before application code runs with flags

The hooks can be registered before the application code is run by using the
[`--import`][] or [`--require`][] flag:

```bash
node --import ./register-hooks.js ./my-app.js
node --require ./register-hooks.js ./my-app.js
```

The specifier passed to `--import` or `--require` can also come from a package:

```bash
node --import some-package/register ./my-app.js
node --require some-package/register ./my-app.js
```

Where `some-package` has an [`"exports"`][] field defining the `/register`
export to map to a file that calls `registerHooks()`, like the
`register-hooks.js` examples above.

Using `--import` or `--require` ensures that the hooks are registered before any
application code is loaded, including the entry point of the application and for
any worker threads by default as well.

##### Registering hooks before application code runs programmatically

Alternatively, `registerHooks()` can be called from the entry point.

If the entry point needs to load other modules and the loading process needs to be
customized, load them using either `require()` or dynamic `import()` after the hooks
are registered. Do not use static `import` statements to load modules that need to be
customized in the same module that registers the hooks, because static `import` statements
are evaluated before any code in the importer module is run, including the call to
`registerHooks()`, regardless of where the static `import` statements appear in the importer
module.

```mjs
import { registerHooks } from 'node:module';

registerHooks({ /* implementation of synchronous hooks */ });

// If loaded using static import, the hooks would not be applied when loading
// my-app.mjs, because statically imported modules are all executed before its
// importer regardless of where the static import appears.
// import './my-app.mjs';

// my-app.mjs must be loaded dynamically to ensure the hooks are applied.
await import('./my-app.mjs');
```

```cjs
const { registerHooks } = require('node:module');

registerHooks({ /* implementation of synchronous hooks */ });

import('./my-app.mjs');
// Or, if my-app.mjs does not have top-level await or it's a CommonJS module,
// require() can also be used:
// require('./my-app.mjs');
```

##### Registering hooks before application code runs with a `data:` URL

Alternatively, inline JavaScript code can be embedded in `data:` URLs to register
the hooks before the application code runs. For example,

```bash
node --import 'data:text/javascript,import {registerHooks} from "node:module"; registerHooks(/* hooks code */);' ./my-app.js
```

#### Convention of hooks and chaining

Hooks are part of a chain, even if that chain consists of only one
custom (user-provided) hook and the default hook, which is always present.

Hook functions nest: each one must always return a plain object, and chaining happens
as a result of each function calling `next<hookName>()`, which is a reference to
the subsequent loader's hook (in LIFO order).

It's possible to call `registerHooks()` more than once:

```mjs
// entrypoint.mjs
import { registerHooks } from 'node:module';

const hook1 = { /* implementation of hooks */ };
const hook2 = { /* implementation of hooks */ };
// hook2 runs before hook1.
registerHooks(hook1);
registerHooks(hook2);
```

```cjs
// entrypoint.cjs
const { registerHooks } = require('node:module');

const hook1 = { /* implementation of hooks */ };
const hook2 = { /* implementation of hooks */ };
// hook2 runs before hook1.
registerHooks(hook1);
registerHooks(hook2);
```

In this example, the registered hooks will form chains. These chains run
last-in, first-out (LIFO). If both `hook1` and `hook2` define a `resolve`
hook, they will be called like so (note the right-to-left,
starting with `hook2.resolve`, then `hook1.resolve`, then the Node.js default):

Node.js default `resolve` ← `hook1.resolve` ← `hook2.resolve`

The same applies to all the other hooks.

A hook that returns a value lacking a required property triggers an exception. A
hook that returns without calling `next<hookName>()` _and_ without returning
`shortCircuit: true` also triggers an exception. These errors are to help
prevent unintentional breaks in the chain. Return `shortCircuit: true` from a
hook to signal that the chain is intentionally ending at your hook.

If a hook should be applied when loading other hook modules, the other hook
modules should be loaded after the hook is registered.

#### Hook functions accepted by `module.registerHooks()`

<!-- YAML
added:
  - v23.5.0
  - v22.15.0
-->

The `module.registerHooks()` method accepts the following synchronous hook functions.

```mjs
function resolve(specifier, context, nextResolve) {
  // Take an `import` or `require` specifier and resolve it to a URL.
}

function load(url, context, nextLoad) {
  // Take a resolved URL and return the source code to be evaluated.
}
```

Synchronous hooks are run in the same thread and the same [realm][] where the modules
are loaded, the code in the hook function can pass values to the modules being referenced
directly via global variables or other shared states.

Unlike the asynchronous hooks, the synchronous hooks are not inherited into child worker
threads by default, though if the hooks are registered using a file preloaded by
[`--import`][] or [`--require`][], child worker threads can inherit the preloaded scripts
via `process.execArgv` inheritance. See [the documentation of `Worker`][] for details.

#### Synchronous `resolve(specifier, context, nextResolve)`

<!-- YAML
changes:
  - version:
    - v23.5.0
    - v22.15.0
    pr-url: https://github.com/nodejs/node/pull/55698
    description: Add support for synchronous and in-thread hooks.
-->

* `specifier` {string}
* `context` {Object}
  * `conditions` {string\[]} Export conditions of the relevant `package.json`
  * `importAttributes` {Object} An object whose key-value pairs represent the
    attributes for the module to import
  * `parentURL` {string|undefined} The module importing this one, or undefined
    if this is the Node.js entry point
* `nextResolve` {Function} The subsequent `resolve` hook in the chain, or the
  Node.js default `resolve` hook after the last user-supplied `resolve` hook
  * `specifier` {string}
  * `context` {Object|undefined} When omitted, the defaults are provided. When provided, defaults
    are merged in with preference to the provided properties.
* Returns: {Object}
  * `format` {string|null|undefined} A hint to the `load` hook (it might be ignored). It can be a
    module format (such as `'commonjs'` or `'module'`) or an arbitrary value like `'css'` or
    `'yaml'`.
  * `importAttributes` {Object|undefined} The import attributes to use when
    caching the module (optional; if excluded the input will be used)
  * `shortCircuit` {undefined|boolean} A signal that this hook intends to
    terminate the chain of `resolve` hooks. **Default:** `false`
  * `url` {string} The absolute URL to which this input resolves

The `resolve` hook chain is responsible for telling Node.js where to find and
how to cache a given `import` statement or expression, or `require` call. It can
optionally return a format (such as `'module'`) as a hint to the `load` hook. If
a format is specified, the `load` hook is ultimately responsible for providing
the final `format` value (and it is free to ignore the hint provided by
`resolve`); if `resolve` provides a `format`, a custom `load` hook is required
even if only to pass the value to the Node.js default `load` hook.

Import type attributes are part of the cache key for saving loaded modules into
the internal module cache. The `resolve` hook is responsible for returning an
`importAttributes` object if the module should be cached with different
attributes than were present in the source code.

The `conditions` property in `context` is an array of conditions that will be used
to match [package exports conditions][Conditional exports] for this resolution
request. They can be used for looking up conditional mappings elsewhere or to
modify the list when calling the default resolution logic.

The current [package exports conditions][Conditional exports] are always in
the `context.conditions` array passed into the hook. To guarantee _default
Node.js module specifier resolution behavior_ when calling `defaultResolve`, the
`context.conditions` array passed to it _must_ include _all_ elements of the
`context.conditions` array originally passed into the `resolve` hook.

```mjs
import { registerHooks } from 'node:module';

function resolve(specifier, context, nextResolve) {
  // When calling `defaultResolve`, the arguments can be modified. For example,
  // to change the specifier or to add applicable export conditions.
  if (specifier.includes('foo')) {
    specifier = specifier.replace('foo', 'bar');
    return nextResolve(specifier, {
      ...context,
      conditions: [...context.conditions, 'another-condition'],
    });
  }

  // The hook can also skip default resolution and provide a custom URL.
  if (specifier === 'special-module') {
    return {
      url: 'file:///path/to/special-module.mjs',
      format: 'module',
      shortCircuit: true,  // This is mandatory if nextResolve() is not called.
    };
  }

  // If no customization is needed, defer to the next hook in the chain which would be the
  // Node.js default resolve if this is the last user-specified loader.
  return nextResolve(specifier);
}

registerHooks({ resolve });
```

#### Synchronous `load(url, context, nextLoad)`

<!-- YAML
changes:
  - version:
    - v23.5.0
    - v22.15.0
    pr-url: https://github.com/nodejs/node/pull/55698
    description: Add support for synchronous and in-thread version.
-->

* `url` {string} The URL returned by the `resolve` chain
* `context` {Object}
  * `conditions` {string\[]} Export conditions of the relevant `package.json`
  * `format` {string|null|undefined} The format optionally supplied by the
    `resolve` hook chain. This can be any string value as an input; input values do not need to
    conform to the list of acceptable return values described below.
  * `importAttributes` {Object}
* `nextLoad` {Function} The subsequent `load` hook in the chain, or the
  Node.js default `load` hook after the last user-supplied `load` hook
  * `url` {string}
  * `context` {Object|undefined} When omitted, defaults are provided. When provided, defaults are
    merged in with preference to the provided properties. In the default `nextLoad`, if
    the module pointed to by `url` does not have explicit module type information,
    `context.format` is mandatory.
    <!-- TODO(joyeecheung): make it at least optionally non-mandatory by allowing
         JS-style/TS-style module detection when the format is simply unknown -->
* Returns: {Object}
  * `format` {string} One of the acceptable module formats listed [below][accepted final formats].
  * `shortCircuit` {undefined|boolean} A signal that this hook intends to
    terminate the chain of `load` hooks. **Default:** `false`
  * `source` {string|ArrayBuffer|TypedArray} The source for Node.js to evaluate

The `load` hook provides a way to define a custom method for retrieving the
source code of a resolved URL. This would allow a loader to potentially avoid
reading files from disk. It could also be used to map an unrecognized format to
a supported one, for example `yaml` to `module`.

```mjs
import { registerHooks } from 'node:module';
import { Buffer } from 'node:buffer';

function load(url, context, nextLoad) {
  // The hook can skip default loading and provide a custom source code.
  if (url === 'special-module') {
    return {
      source: 'export const special = 42;',
      format: 'module',
      shortCircuit: true,  // This is mandatory if nextLoad() is not called.
    };
  }

  // It's possible to modify the source code loaded by the next - possibly default - step,
  // for example, replacing 'foo' with 'bar' in the source code of the module.
  const result = nextLoad(url, context);
  const source = typeof result.source === 'string' ?
    result.source : Buffer.from(result.source).toString('utf8');
  return {
    source: source.replace(/foo/g, 'bar'),
    ...result,
  };
}

registerHooks({ resolve });
```

In a more advanced scenario, this can also be used to transform an unsupported
source to a supported one (see [Examples](#examples) below).

##### Accepted final formats returned by `load`

The final value of `format` must be one of the following:

| `format`                | Description                                           | Acceptable types for `source` returned by `load`   |
| ----------------------- | ----------------------------------------------------- | -------------------------------------------------- |
| `'addon'`               | Load a Node.js addon                                  | {null}                                             |
| `'builtin'`             | Load a Node.js builtin module                         | {null}                                             |
| `'commonjs-typescript'` | Load a Node.js CommonJS module with TypeScript syntax | {string\|ArrayBuffer\|TypedArray\|null\|undefined} |
| `'commonjs'`            | Load a Node.js CommonJS module                        | {string\|ArrayBuffer\|TypedArray\|null\|undefined} |
| `'json'`                | Load a JSON file                                      | {string\|ArrayBuffer\|TypedArray}                  |
| `'module-typescript'`   | Load an ES module with TypeScript syntax              | {string\|ArrayBuffer\|TypedArray}                  |
| `'module'`              | Load an ES module                                     | {string\|ArrayBuffer\|TypedArray}                  |
| `'wasm'`                | Load a WebAssembly module                             | {ArrayBuffer\|TypedArray}                          |

The value of `source` is ignored for format `'builtin'` because currently it is
not possible to replace the value of a Node.js builtin (core) module.

> These types all correspond to classes defined in ECMAScript.

* The specific {ArrayBuffer} object is a {SharedArrayBuffer}.
* The specific {TypedArray} object is a {Uint8Array}.

If the source value of a text-based format (i.e., `'json'`, `'module'`)
is not a string, it is converted to a string using [`util.TextDecoder`][].

### Asynchronous customization hooks

> Stability: 1.1 - Active Development

#### Caveats of asynchronous customization hooks

The asynchronous customization hooks have many caveats and it is uncertain if their
issues can be resolved. Users are encouraged to use the synchronous customization hooks
via `module.registerHooks()` instead to avoid these caveats.

* Asynchronous hooks run on a separate thread, so the hook functions cannot directly
  mutate the global state of the modules being customized. It's typical to use message
  channels and atomics to pass data between the two or to affect control flows.
  See [Communication with asynchronous module customization hooks](#communication-with-asynchronous-module-customization-hooks).
* Asynchronous hooks do not affect all `require()` calls in the module graph.
  * Custom `require` functions created using `module.createRequire()` are not
    affected.
  * If the asynchronous `load` hook does not override the `source` for CommonJS modules
    that go through it, the child modules loaded by those CommonJS modules via built-in
    `require()` would not be affected by the asynchronous hooks either.
* There are several caveats that the asynchronous hooks need to handle when
  customizing CommonJS modules. See [asynchronous `resolve` hook][] and
  [asynchronous `load` hook][] for details.
* When `require()` calls inside CommonJS modules are customized by asynchronous hooks,
  Node.js may need to load the source code of the CommonJS module multiple times to maintain
  compatibility with existing CommonJS monkey-patching. If the module code changes between
  loads, this may lead to unexpected behaviors.
  * As a side effect, if both asynchronous hooks and synchronous hooks are registered and the
    asynchronous hooks choose to customize the CommonJS module, the synchronous hooks may be
    invoked multiple times for the `require()` calls in that CommonJS module.

#### Registration of asynchronous customization hooks

Asynchronous customization hooks are registered using [`module.register()`][`register`] which takes
a path or URL to another module that exports the [asynchronous hook functions][].

Similar to `registerHooks()`, `register()` can be called in a module preloaded by `--import` or
`--require`, or called directly within the entry point.

```mjs
// Use module.register() to register asynchronous hooks in a dedicated thread.
import { register } from 'node:module';
register('./hooks.mjs', import.meta.url);

// If my-app.mjs is loaded statically here as `import './my-app.mjs'`, since ESM
// dependencies are evaluated before the module that imports them,
// it's loaded _before_ the hooks are registered above and won't be affected.
// To ensure the hooks are applied, dynamic import() must be used to load ESM
// after the hooks are registered.
import('./my-app.mjs');
```

```cjs
const { register } = require('node:module');
const { pathToFileURL } = require('node:url');
// Use module.register() to register asynchronous hooks in a dedicated thread.
register('./hooks.mjs', pathToFileURL(__filename));

import('./my-app.mjs');
```

In `hooks.mjs`:

```mjs
// hooks.mjs
export async function resolve(specifier, context, nextResolve) {
  /* implementation */
}
export async function load(url, context, nextLoad) {
  /* implementation */
}
```

Unlike synchronous hooks, the asynchronous hooks would not run for these modules loaded in the file
that calls `register()`:

<!-- eslint-disable no-restricted-globals -->

```mjs
// register-hooks.js
import { register, createRequire } from 'node:module';
register('./hooks.mjs', import.meta.url);

// Asynchronous hooks does not affect modules loaded via custom require()
// functions created by module.createRequire().
const userRequire = createRequire(__filename);
userRequire('./my-app-2.cjs');  // Hooks won't affect this
```

<!-- eslint-enable no-restricted-globals -->

```cjs
// register-hooks.js
const { register, createRequire } = require('node:module');
const { pathToFileURL } = require('node:url');
register('./hooks.mjs', pathToFileURL(__filename));

// Asynchronous hooks does not affect modules loaded via built-in require()
// in the module calling `register()`
require('./my-app-2.cjs');  // Hooks won't affect this
// .. or custom require() functions created by module.createRequire().
const userRequire = createRequire(__filename);
userRequire('./my-app-3.cjs');  // Hooks won't affect this
```

Asynchronous hooks can also be registered using a `data:` URL with the `--import` flag:

```bash
node --import 'data:text/javascript,import { register } from "node:module"; import { pathToFileURL } from "node:url"; register("my-instrumentation", pathToFileURL("./"));' ./my-app.js
```

#### Chaining of asynchronous customization hooks

Chaining of `register()` work similarly to `registerHooks()`. If synchronous and asynchronous
hooks are mixed, the synchronous hooks are always run first before the asynchronous
hooks start running, that is, in the last synchronous hook being run, its next
hook includes invocation of the asynchronous hooks.

```mjs
// entrypoint.mjs
import { register } from 'node:module';

register('./foo.mjs', import.meta.url);
register('./bar.mjs', import.meta.url);
await import('./my-app.mjs');
```

```cjs
// entrypoint.cjs
const { register } = require('node:module');
const { pathToFileURL } = require('node:url');

const parentURL = pathToFileURL(__filename);
register('./foo.mjs', parentURL);
register('./bar.mjs', parentURL);
import('./my-app.mjs');
```

If `foo.mjs` and `bar.mjs` define a `resolve` hook, they will be called like so
(note the right-to-left, starting with `./bar.mjs`, then `./foo.mjs`, then the Node.js default):

Node.js default ← `./foo.mjs` ← `./bar.mjs`

When using the asynchronous hooks, the registered hooks also affect subsequent
`register` calls, which takes care of loading hook modules. In the example above,
`bar.mjs` will be resolved and loaded via the hooks registered by `foo.mjs`
(because `foo`'s hooks will have already been added to the chain). This allows
for things like writing hooks in non-JavaScript languages, so long as
earlier registered hooks transpile into JavaScript.

The `register()` method cannot be called from the thread running the hook module that
exports the asynchronous hooks or its dependencies.

#### Communication with asynchronous module customization hooks

Asynchronous hooks run on a dedicated thread, separate from the main
thread that runs application code. This means mutating global variables won't
affect the other thread(s), and message channels must be used to communicate
between the threads.

The `register` method can be used to pass data to an [`initialize`][] hook. The
data passed to the hook may include transferable objects like ports.

```mjs
import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

// This example demonstrates how a message channel can be used to
// communicate with the hooks, by sending `port2` to the hooks.
const { port1, port2 } = new MessageChannel();

port1.on('message', (msg) => {
  console.log(msg);
});
port1.unref();

register('./my-hooks.mjs', {
  parentURL: import.meta.url,
  data: { number: 1, port: port2 },
  transferList: [port2],
});
```

```cjs
const { register } = require('node:module');
const { pathToFileURL } = require('node:url');
const { MessageChannel } = require('node:worker_threads');

// This example showcases how a message channel can be used to
// communicate with the hooks, by sending `port2` to the hooks.
const { port1, port2 } = new MessageChannel();

port1.on('message', (msg) => {
  console.log(msg);
});
port1.unref();

register('./my-hooks.mjs', {
  parentURL: pathToFileURL(__filename),
  data: { number: 1, port: port2 },
  transferList: [port2],
});
```

#### Asynchronous hooks accepted by `module.register()`

<!-- YAML
added: v8.8.0
changes:
  - version:
    - v20.6.0
    - v18.19.0
    pr-url: https://github.com/nodejs/node/pull/48842
    description: Added `initialize` hook to replace `globalPreload`.
  - version:
    - v18.6.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42623
    description: Add support for chaining loaders.
  - version: v16.12.0
    pr-url: https://github.com/nodejs/node/pull/37468
    description: Removed `getFormat`, `getSource`, `transformSource`, and
                 `globalPreload`; added `load` hook and `getGlobalPreload` hook.
-->

The [`register`][] method can be used to register a module that exports a set of
hooks. The hooks are functions that are called by Node.js to customize the
module resolution and loading process. The exported functions must have specific
names and signatures, and they must be exported as named exports.

```mjs
export async function initialize({ number, port }) {
  // Receives data from `register`.
}

export async function resolve(specifier, context, nextResolve) {
  // Take an `import` or `require` specifier and resolve it to a URL.
}

export async function load(url, context, nextLoad) {
  // Take a resolved URL and return the source code to be evaluated.
}
```

Asynchronous hooks are run in a separate thread, isolated from the main thread where
application code runs. That means it is a different [realm][]. The hooks thread
may be terminated by the main thread at any time, so do not depend on
asynchronous operations (like `console.log`) to complete. They are inherited into
child workers by default.

#### `initialize()`

<!-- YAML
added:
  - v20.6.0
  - v18.19.0
-->

* `data` {any} The data from `register(loader, import.meta.url, { data })`.

The `initialize` hook is only accepted by [`register`][]. `registerHooks()` does
not support nor need it since initialization done for synchronous hooks can be run
directly before the call to `registerHooks()`.

The `initialize` hook provides a way to define a custom function that runs in
the hooks thread when the hooks module is initialized. Initialization happens
when the hooks module is registered via [`register`][].

This hook can receive data from a [`register`][] invocation, including
ports and other transferable objects. The return value of `initialize` can be a
{Promise}, in which case it will be awaited before the main application thread
execution resumes.

Module customization code:

```mjs
// path-to-my-hooks.js

export async function initialize({ number, port }) {
  port.postMessage(`increment: ${number + 1}`);
}
```

Caller code:

```mjs
import assert from 'node:assert';
import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

// This example showcases how a message channel can be used to communicate
// between the main (application) thread and the hooks running on the hooks
// thread, by sending `port2` to the `initialize` hook.
const { port1, port2 } = new MessageChannel();

port1.on('message', (msg) => {
  assert.strictEqual(msg, 'increment: 2');
});
port1.unref();

register('./path-to-my-hooks.js', {
  parentURL: import.meta.url,
  data: { number: 1, port: port2 },
  transferList: [port2],
});
```

```cjs
const assert = require('node:assert');
const { register } = require('node:module');
const { pathToFileURL } = require('node:url');
const { MessageChannel } = require('node:worker_threads');

// This example showcases how a message channel can be used to communicate
// between the main (application) thread and the hooks running on the hooks
// thread, by sending `port2` to the `initialize` hook.
const { port1, port2 } = new MessageChannel();

port1.on('message', (msg) => {
  assert.strictEqual(msg, 'increment: 2');
});
port1.unref();

register('./path-to-my-hooks.js', {
  parentURL: pathToFileURL(__filename),
  data: { number: 1, port: port2 },
  transferList: [port2],
});
```

#### Asynchronous `resolve(specifier, context, nextResolve)`

<!-- YAML
changes:
  - version:
    - v21.0.0
    - v20.10.0
    - v18.19.0
    pr-url: https://github.com/nodejs/node/pull/50140
    description: The property `context.importAssertions` is replaced with
                 `context.importAttributes`. Using the old name is still
                 supported and will emit an experimental warning.
  - version:
    - v18.6.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42623
    description: Add support for chaining resolve hooks. Each hook must either
      call `nextResolve()` or include a `shortCircuit` property set to `true`
      in its return.
  - version:
    - v17.1.0
    - v16.14.0
    pr-url: https://github.com/nodejs/node/pull/40250
    description: Add support for import assertions.
-->

* `specifier` {string}
* `context` {Object}
  * `conditions` {string\[]} Export conditions of the relevant `package.json`
  * `importAttributes` {Object} An object whose key-value pairs represent the
    attributes for the module to import
  * `parentURL` {string|undefined} The module importing this one, or undefined
    if this is the Node.js entry point
* `nextResolve` {Function} The subsequent `resolve` hook in the chain, or the
  Node.js default `resolve` hook after the last user-supplied `resolve` hook
  * `specifier` {string}
  * `context` {Object|undefined} When omitted, the defaults are provided. When provided, defaults
    are merged in with preference to the provided properties.
* Returns: {Object|Promise} The asynchronous version takes either an object containing the
  following properties, or a `Promise` that will resolve to such an object.
  * `format` {string|null|undefined} A hint to the `load` hook (it might be ignored). It can be a
    module format (such as `'commonjs'` or `'module'`) or an arbitrary value like `'css'` or
    `'yaml'`.
  * `importAttributes` {Object|undefined} The import attributes to use when
    caching the module (optional; if excluded the input will be used)
  * `shortCircuit` {undefined|boolean} A signal that this hook intends to
    terminate the chain of `resolve` hooks. **Default:** `false`
  * `url` {string} The absolute URL to which this input resolves

The asynchronous version works similarly to the synchronous version, only that the
`nextResolve` function returns a `Promise`, and the `resolve` hook itself can return a `Promise`.

> **Warning** In the case of the asynchronous version, despite support for returning
> promises and async functions, calls to `resolve` may still block the main thread which
> can impact performance.

> **Warning** The `resolve` hook invoked for `require()` calls inside CommonJS modules
> customized by asynchronous hooks does not receive the original specifier passed to
> `require()`. Instead, it receives a URL already fully resolved using the default
> CommonJS resolution.

> **Warning** In the CommonJS modules that are customized by the asynchronous customization hooks,
> `require.resolve()` and `require()` will use `"import"` export condition instead of
> `"require"`, which may cause unexpected behaviors when loading dual packages.

```mjs
export async function resolve(specifier, context, nextResolve) {
  // When calling `defaultResolve`, the arguments can be modified. For example,
  // to change the specifier or add conditions.
  if (specifier.includes('foo')) {
    specifier = specifier.replace('foo', 'bar');
    return nextResolve(specifier, {
      ...context,
      conditions: [...context.conditions, 'another-condition'],
    });
  }

  // The hook can also skips default resolution and provide a custom URL.
  if (specifier === 'special-module') {
    return {
      url: 'file:///path/to/special-module.mjs',
      format: 'module',
      shortCircuit: true,  // This is mandatory if not calling nextResolve().
    };
  }

  // If no customization is needed, defer to the next hook in the chain which would be the
  // Node.js default resolve if this is the last user-specified loader.
  return nextResolve(specifier);
}
```

#### Asynchronous `load(url, context, nextLoad)`

<!-- YAML
changes:
  - version: v22.6.0
    pr-url: https://github.com/nodejs/node/pull/56350
    description: Add support for `source` with format `commonjs-typescript` and `module-typescript`.
  - version: v20.6.0
    pr-url: https://github.com/nodejs/node/pull/47999
    description: Add support for `source` with format `commonjs`.
  - version:
    - v18.6.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42623
    description: Add support for chaining load hooks. Each hook must either
      call `nextLoad()` or include a `shortCircuit` property set to `true` in
      its return.
-->

* `url` {string} The URL returned by the `resolve` chain
* `context` {Object}
  * `conditions` {string\[]} Export conditions of the relevant `package.json`
  * `format` {string|null|undefined} The format optionally supplied by the
    `resolve` hook chain. This can be any string value as an input; input values do not need to
    conform to the list of acceptable return values described below.
  * `importAttributes` {Object}
* `nextLoad` {Function} The subsequent `load` hook in the chain, or the
  Node.js default `load` hook after the last user-supplied `load` hook
  * `url` {string}
  * `context` {Object|undefined} When omitted, defaults are provided. When provided, defaults are
    merged in with preference to the provided properties. In the default `nextLoad`, if
    the module pointed to by `url` does not have explicit module type information,
    `context.format` is mandatory.
    <!-- TODO(joyeecheung): make it at least optionally non-mandatory by allowing
         JS-style/TS-style module detection when the format is simply unknown -->
* Returns: {Promise} The asynchronous version takes either an object containing the
  following properties, or a `Promise` that will resolve to such an object.
  * `format` {string}
  * `shortCircuit` {undefined|boolean} A signal that this hook intends to
    terminate the chain of `load` hooks. **Default:** `false`
  * `source` {string|ArrayBuffer|TypedArray} The source for Node.js to evaluate

> **Warning**: The asynchronous `load` hook and namespaced exports from CommonJS
> modules are incompatible. Attempting to use them together will result in an empty
> object from the import. This may be addressed in the future. This does not apply
> to the synchronous `load` hook, in which case exports can be used as usual.

The asynchronous version works similarly to the synchronous version, though
when using the asynchronous `load` hook, omitting vs providing a `source` for
`'commonjs'` has very different effects:

* When a `source` is provided, all `require` calls from this module will be
  processed by the ESM loader with registered `resolve` and `load` hooks; all
  `require.resolve` calls from this module will be processed by the ESM loader
  with registered `resolve` hooks; only a subset of the CommonJS API will be
  available (e.g. no `require.extensions`, no `require.cache`, no
  `require.resolve.paths`) and monkey-patching on the CommonJS module loader
  will not apply.
* If `source` is undefined or `null`, it will be handled by the CommonJS module
  loader and `require`/`require.resolve` calls will not go through the
  registered hooks. This behavior for nullish `source` is temporary — in the
  future, nullish `source` will not be supported.

These caveats do not apply to the synchronous `load` hook, in which case
the complete set of CommonJS APIs available to the customized CommonJS
modules, and `require`/`require.resolve` always go through the registered
hooks.

The Node.js internal asynchronous `load` implementation, which is the value of `next` for the
last hook in the `load` chain, returns `null` for `source` when `format` is
`'commonjs'` for backward compatibility. Here is an example hook that would
opt-in to using the non-default behavior:

```mjs
import { readFile } from 'node:fs/promises';

// Asynchronous version accepted by module.register(). This fix is not needed
// for the synchronous version accepted by module.registerHooks().
export async function load(url, context, nextLoad) {
  const result = await nextLoad(url, context);
  if (result.format === 'commonjs') {
    result.source ??= await readFile(new URL(result.responseURL ?? url));
  }
  return result;
}
```

This doesn't apply to the synchronous `load` hook either, in which case the
`source` returned contains source code loaded by the next hook, regardless
of module format.

### Examples

The various module customization hooks can be used together to accomplish
wide-ranging customizations of the Node.js code loading and evaluation
behaviors.

#### Import from HTTPS

The hook below registers hooks to enable rudimentary support for such
specifiers. While this may seem like a significant improvement to Node.js core
functionality, there are substantial downsides to actually using these hooks:
performance is much slower than loading files from disk, there is no caching,
and there is no security.

```mjs
// https-hooks.mjs
import { get } from 'node:https';

export function load(url, context, nextLoad) {
  // For JavaScript to be loaded over the network, we need to fetch and
  // return it.
  if (url.startsWith('https://')) {
    return new Promise((resolve, reject) => {
      get(url, (res) => {
        let data = '';
        res.setEncoding('utf8');
        res.on('data', (chunk) => data += chunk);
        res.on('end', () => resolve({
          // This example assumes all network-provided JavaScript is ES module
          // code.
          format: 'module',
          shortCircuit: true,
          source: data,
        }));
      }).on('error', (err) => reject(err));
    });
  }

  // Let Node.js handle all other URLs.
  return nextLoad(url);
}
```

```mjs
// main.mjs
import { VERSION } from 'https://coffeescript.org/browser-compiler-modern/coffeescript.js';

console.log(VERSION);
```

With the preceding hooks module, running
`node --import 'data:text/javascript,import { register } from "node:module"; import { pathToFileURL } from "node:url"; register(pathToFileURL("./https-hooks.mjs"));' ./main.mjs`
prints the current version of CoffeeScript per the module at the URL in
`main.mjs`.

<!-- TODO(joyeecheung): add an example on how to implement it with a fetchSync based on
workers and Atomics.wait() - or all these examples are too much to be put in the API
documentation already and should be put into a repository instead? -->

#### Transpilation

Sources that are in formats Node.js doesn't understand can be converted into
JavaScript using the [`load` hook][load hook].

This is less performant than transpiling source files before running Node.js;
transpiler hooks should only be used for development and testing purposes.

##### Asynchronous version

```mjs
// coffeescript-hooks.mjs
import { readFile } from 'node:fs/promises';
import { findPackageJSON } from 'node:module';
import coffeescript from 'coffeescript';

const extensionsRegex = /\.(coffee|litcoffee|coffee\.md)$/;

export async function load(url, context, nextLoad) {
  if (extensionsRegex.test(url)) {
    // CoffeeScript files can be either CommonJS or ES modules. Use a custom format
    // to tell Node.js not to detect its module type.
    const { source: rawSource } = await nextLoad(url, { ...context, format: 'coffee' });
    // This hook converts CoffeeScript source code into JavaScript source code
    // for all imported CoffeeScript files.
    const transformedSource = coffeescript.compile(rawSource.toString(), url);

    // To determine how Node.js would interpret the transpilation result,
    // search up the file system for the nearest parent package.json file
    // and read its "type" field.
    return {
      format: await getPackageType(url),
      shortCircuit: true,
      source: transformedSource,
    };
  }

  // Let Node.js handle all other URLs.
  return nextLoad(url, context);
}

async function getPackageType(url) {
  // `url` is only a file path during the first iteration when passed the
  // resolved url from the load() hook
  // an actual file path from load() will contain a file extension as it's
  // required by the spec
  // this simple truthy check for whether `url` contains a file extension will
  // work for most projects but does not cover some edge-cases (such as
  // extensionless files or a url ending in a trailing space)
  const pJson = findPackageJSON(url);

  return readFile(pJson, 'utf8')
    .then(JSON.parse)
    .then((json) => json?.type)
    .catch(() => undefined);
}
```

##### Synchronous version

```mjs
// coffeescript-sync-hooks.mjs
import { readFileSync } from 'node:fs';
import { registerHooks, findPackageJSON } from 'node:module';
import coffeescript from 'coffeescript';

const extensionsRegex = /\.(coffee|litcoffee|coffee\.md)$/;

function load(url, context, nextLoad) {
  if (extensionsRegex.test(url)) {
    const { source: rawSource } = nextLoad(url, { ...context, format: 'coffee' });
    const transformedSource = coffeescript.compile(rawSource.toString(), url);

    return {
      format: getPackageType(url),
      shortCircuit: true,
      source: transformedSource,
    };
  }

  return nextLoad(url, context);
}

function getPackageType(url) {
  const pJson = findPackageJSON(url);
  if (!pJson) {
    return undefined;
  }
  try {
    const file = readFileSync(pJson, 'utf-8');
    return JSON.parse(file)?.type;
  } catch {
    return undefined;
  }
}

registerHooks({ load });
```

#### Running hooks

```coffee
# main.coffee
import { scream } from './scream.coffee'
console.log scream 'hello, world'

import { version } from 'node:process'
console.log "Brought to you by Node.js version #{version}"
```

```coffee
# scream.coffee
export scream = (str) -> str.toUpperCase()
```

For the sake of running the example, add a `package.json` file containing the
module type of the CoffeeScript files.

```json
{
  "type": "module"
}
```

This is only for running the example. In real world loaders, `getPackageType()` must be
able to return an `format` known to Node.js even in the absence of an explicit type in a
`package.json`, or otherwise the `nextLoad` call would throw `ERR_UNKNOWN_FILE_EXTENSION`
(if undefined) or `ERR_UNKNOWN_MODULE_FORMAT` (if it's not a known format listed in
the [load hook][] documentation).

With the preceding hooks modules, running
`node --import 'data:text/javascript,import { register } from "node:module"; import { pathToFileURL } from "node:url"; register(pathToFileURL("./coffeescript-hooks.mjs"));' ./main.coffee`
or `node --import ./coffeescript-sync-hooks.mjs ./main.coffee`
causes `main.coffee` to be turned into JavaScript after its source code is
loaded from disk but before Node.js executes it; and so on for any `.coffee`,
`.litcoffee` or `.coffee.md` files referenced via `import` statements of any
loaded file.

#### Import maps

The previous two examples defined `load` hooks. This is an example of a
`resolve` hook. This hooks module reads an `import-map.json` file that defines
which specifiers to override to other URLs (this is a very simplistic
implementation of a small subset of the "import maps" specification).

##### Asynchronous version

```mjs
// import-map-hooks.js
import fs from 'node:fs/promises';

const { imports } = JSON.parse(await fs.readFile('import-map.json'));

export async function resolve(specifier, context, nextResolve) {
  if (Object.hasOwn(imports, specifier)) {
    return nextResolve(imports[specifier], context);
  }

  return nextResolve(specifier, context);
}
```

##### Synchronous version

```mjs
// import-map-sync-hooks.js
import fs from 'node:fs/promises';
import module from 'node:module';

const { imports } = JSON.parse(fs.readFileSync('import-map.json', 'utf-8'));

function resolve(specifier, context, nextResolve) {
  if (Object.hasOwn(imports, specifier)) {
    return nextResolve(imports[specifier], context);
  }

  return nextResolve(specifier, context);
}

module.registerHooks({ resolve });
```

##### Using the hooks

With these files:

```mjs
// main.js
import 'a-module';
```

```json
// import-map.json
{
  "imports": {
    "a-module": "./some-module.js"
  }
}
```

```mjs
// some-module.js
console.log('some module!');
```

Running `node --import 'data:text/javascript,import { register } from "node:module"; import { pathToFileURL } from "node:url"; register(pathToFileURL("./import-map-hooks.js"));' main.js`
or `node --import ./import-map-sync-hooks.js main.js`
should print `some module!`.

## Source Map Support

<!-- YAML
added:
 - v13.7.0
 - v12.17.0
-->

> Stability: 1 - Experimental

Node.js supports TC39 ECMA-426 [Source Map][] format (it was called Source map
revision 3 format).

The APIs in this section are helpers for interacting with the source map
cache. This cache is populated when source map parsing is enabled and
[source map include directives][] are found in a modules' footer.

To enable source map parsing, Node.js must be run with the flag
[`--enable-source-maps`][], or with code coverage enabled by setting
[`NODE_V8_COVERAGE=dir`][], or be enabled programmatically via
[`module.setSourceMapsSupport()`][].

```mjs
// module.mjs
// In an ECMAScript module
import { findSourceMap, SourceMap } from 'node:module';
```

```cjs
// module.cjs
// In a CommonJS module
const { findSourceMap, SourceMap } = require('node:module');
```

### `module.getSourceMapsSupport()`

<!-- YAML
added:
  - v23.7.0
  - v22.14.0
-->

* Returns: {Object}
  * `enabled` {boolean} If the source maps support is enabled
  * `nodeModules` {boolean} If the support is enabled for files in `node_modules`.
  * `generatedCode` {boolean} If the support is enabled for generated code from `eval` or `new Function`.

This method returns whether the [Source Map v3][Source Map] support for stack
traces is enabled.

<!-- Anchors to make sure old links find a target -->

<a id="module_module_findsourcemap_path_error"></a>

### `module.findSourceMap(path)`

<!-- YAML
added:
 - v13.7.0
 - v12.17.0
-->

* `path` {string}
* Returns: {module.SourceMap|undefined} Returns `module.SourceMap` if a source
  map is found, `undefined` otherwise.

`path` is the resolved path for the file for which a corresponding source map
should be fetched.

### `module.setSourceMapsSupport(enabled[, options])`

<!-- YAML
added:
  - v23.7.0
  - v22.14.0
-->

* `enabled` {boolean} Enable the source map support.
* `options` {Object} Optional
  * `nodeModules` {boolean} If enabling the support for files in
    `node_modules`. **Default:** `false`.
  * `generatedCode` {boolean} If enabling the support for generated code from
    `eval` or `new Function`. **Default:** `false`.

This function enables or disables the [Source Map v3][Source Map] support for
stack traces.

It provides same features as launching Node.js process with commandline options
`--enable-source-maps`, with additional options to alter the support for files
in `node_modules` or generated codes.

Only source maps in JavaScript files that are loaded after source maps has been
enabled will be parsed and loaded. Preferably, use the commandline options
`--enable-source-maps` to avoid losing track of source maps of modules loaded
before this API call.

### Class: `module.SourceMap`

<!-- YAML
added:
 - v13.7.0
 - v12.17.0
-->

#### `new SourceMap(payload[, { lineLengths }])`

<!-- YAML
changes:
  - version: v20.5.0
    pr-url: https://github.com/nodejs/node/pull/48461
    description: Add support for `lineLengths`.
-->

* `payload` {Object}
* `lineLengths` {number\[]}

Creates a new `sourceMap` instance.

`payload` is an object with keys matching the [Source map format][]:

* `file` {string}
* `version` {number}
* `sources` {string\[]}
* `sourcesContent` {string\[]}
* `names` {string\[]}
* `mappings` {string}
* `sourceRoot` {string}

`lineLengths` is an optional array of the length of each line in the
generated code.

#### `sourceMap.payload`

* Returns: {Object}

Getter for the payload used to construct the [`SourceMap`][] instance.

#### `sourceMap.findEntry(lineOffset, columnOffset)`

* `lineOffset` {number} The zero-indexed line number offset in
  the generated source
* `columnOffset` {number} The zero-indexed column number offset
  in the generated source
* Returns: {Object}

Given a line offset and column offset in the generated source
file, returns an object representing the SourceMap range in the
original file if found, or an empty object if not.

The object returned contains the following keys:

* `generatedLine` {number} The line offset of the start of the
  range in the generated source
* `generatedColumn` {number} The column offset of start of the
  range in the generated source
* `originalSource` {string} The file name of the original source,
  as reported in the SourceMap
* `originalLine` {number} The line offset of the start of the
  range in the original source
* `originalColumn` {number} The column offset of start of the
  range in the original source
* `name` {string}

The returned value represents the raw range as it appears in the
SourceMap, based on zero-indexed offsets, _not_ 1-indexed line and
column numbers as they appear in Error messages and CallSite
objects.

To get the corresponding 1-indexed line and column numbers from a
lineNumber and columnNumber as they are reported by Error stacks
and CallSite objects, use `sourceMap.findOrigin(lineNumber,
columnNumber)`

#### `sourceMap.findOrigin(lineNumber, columnNumber)`

<!-- YAML
added:
  - v20.4.0
  - v18.18.0
-->

* `lineNumber` {number} The 1-indexed line number of the call
  site in the generated source
* `columnNumber` {number} The 1-indexed column number
  of the call site in the generated source
* Returns: {Object}

Given a 1-indexed `lineNumber` and `columnNumber` from a call site in
the generated source, find the corresponding call site location
in the original source.

If the `lineNumber` and `columnNumber` provided are not found in any
source map, then an empty object is returned. Otherwise, the
returned object contains the following keys:

* `name` {string|undefined} The name of the range in the
  source map, if one was provided
* `fileName` {string} The file name of the original source, as
  reported in the SourceMap
* `lineNumber` {number} The 1-indexed lineNumber of the
  corresponding call site in the original source
* `columnNumber` {number} The 1-indexed columnNumber of the
  corresponding call site in the original source

[CommonJS]: modules.md
[Conditional exports]: packages.md#conditional-exports
[Customization hooks]: #customization-hooks
[ES Modules]: esm.md
[Permission Model]: permissions.md#permission-model
[Source Map]: https://tc39.es/ecma426/
[Source map format]: https://tc39.es/ecma426/#sec-source-map-format
[V8 JavaScript code coverage]: https://v8project.blogspot.com/2017/12/javascript-code-coverage.html
[V8 code cache]: https://v8.dev/blog/code-caching-for-devs
[`"exports"`]: packages.md#exports
[`--enable-source-maps`]: cli.md#--enable-source-maps
[`--import`]: cli.md#--importmodule
[`--require`]: cli.md#-r---require-module
[`NODE_COMPILE_CACHE=dir`]: cli.md#node_compile_cachedir
[`NODE_COMPILE_CACHE_PORTABLE=1`]: cli.md#node_compile_cache_portable1
[`NODE_DISABLE_COMPILE_CACHE=1`]: cli.md#node_disable_compile_cache1
[`NODE_V8_COVERAGE=dir`]: cli.md#node_v8_coveragedir
[`SourceMap`]: #class-modulesourcemap
[`initialize`]: #initialize
[`module.constants.compileCacheStatus`]: #moduleconstantscompilecachestatus
[`module.enableCompileCache()`]: #moduleenablecompilecacheoptions
[`module.flushCompileCache()`]: #moduleflushcompilecache
[`module.getCompileCacheDir()`]: #modulegetcompilecachedir
[`module.registerHooks()`]: #moduleregisterhooksoptions
[`module.setSourceMapsSupport()`]: #modulesetsourcemapssupportenabled-options
[`module`]: #the-module-object
[`os.tmpdir()`]: os.md#ostmpdir
[`register`]: #moduleregisterspecifier-parenturl-options
[`util.TextDecoder`]: util.md#class-utiltextdecoder
[accepted final formats]: #accepted-final-formats-returned-by-load
[asynchronous `load` hook]: #asynchronous-loadurl-context-nextload
[asynchronous `resolve` hook]: #asynchronous-resolvespecifier-context-nextresolve
[asynchronous hook functions]: #asynchronous-hooks-accepted-by-moduleregister
[caveats of asynchronous customization hooks]: #caveats-of-asynchronous-customization-hooks
[hooks]: #customization-hooks
[load hook]: #synchronous-loadurl-context-nextload
[module compile cache]: #module-compile-cache
[module wrapper]: modules.md#the-module-wrapper
[realm]: https://tc39.es/ecma262/#realm
[resolve hook]: #synchronous-resolvespecifier-context-nextresolve
[source map include directives]: https://tc39.es/ecma426/#sec-linking-generated-code
[synchronous hook functions]: #hook-functions-accepted-by-moduleregisterhooks
[the documentation of `Worker`]: worker_threads.md#new-workerfilename-options
[transferable objects]: worker_threads.md#portpostmessagevalue-transferlist
[transform TypeScript features]: typescript.md#typescript-features
[type-stripping]: typescript.md#type-stripping
