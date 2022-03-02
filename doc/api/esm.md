# Modules: ECMAScript modules

<!--introduced_in=v8.5.0-->

<!-- type=misc -->

<!-- YAML
added: v8.5.0
changes:
  - version: v16.14.0
    pr-url: https://github.com/nodejs/node/pull/40250
    description: Add support for import assertions.
  - version:
    - v16.12.0
    pr-url: https://github.com/nodejs/node/pull/37468
    description:
      Consolidate loader hooks, removed `getFormat`, `getSource`,
      `transformSource`, and `getGlobalPreloadCode` hooks
      added `load` and `globalPreload` hooks
      allowed returning `format` from either `resolve` or `load` hooks.
  - version:
    - v15.3.0
    - v12.22.0
    pr-url: https://github.com/nodejs/node/pull/35781
    description: Stabilize modules implementation.
  - version:
    - v14.13.0
    - v12.20.0
    pr-url: https://github.com/nodejs/node/pull/35249
    description: Support for detection of CommonJS named exports.
  - version: v14.8.0
    pr-url: https://github.com/nodejs/node/pull/34558
    description: Unflag Top-Level Await.
  - version:
    - v14.0.0
    - v13.14.0
    - v12.20.0
    pr-url: https://github.com/nodejs/node/pull/31974
    description: Remove experimental modules warning.
  - version:
    - v13.2.0
    - v12.17.0
    pr-url: https://github.com/nodejs/node/pull/29866
    description: Loading ECMAScript modules no longer requires a command-line flag.
  - version: v12.0.0
    pr-url: https://github.com/nodejs/node/pull/26745
    description:
      Add support for ES modules using `.js` file extension via `package.json`
      `"type"` field.
-->

> Stability: 2 - Stable

## Introduction

<!--name=esm-->

ECMAScript modules are [the official standard format][] to package JavaScript
code for reuse. Modules are defined using a variety of [`import`][] and
[`export`][] statements.

The following example of an ES module exports a function:

```js
// addTwo.mjs
function addTwo(num) {
  return num + 2;
}

export { addTwo };
```

The following example of an ES module imports the function from `addTwo.mjs`:

```js
// app.mjs
import { addTwo } from './addTwo.mjs';

// Prints: 6
console.log(addTwo(4));
```

Node.js fully supports ECMAScript modules as they are currently specified and
provides interoperability between them and its original module format,
[CommonJS][].

<!-- Anchors to make sure old links find a target -->

<i id="esm_package_json_type_field"></i><i id="esm_package_scope_and_file_extensions"></i><i id="esm_input_type_flag"></i>

## Enabling

<!-- type=misc -->

Node.js has two module systems: [CommonJS][] modules and ECMAScript modules.

Authors can tell Node.js to use the ECMAScript modules loader
via the `.mjs` file extension, the `package.json` [`"type"`][] field, or the
[`--input-type`][] flag. Outside of those cases, Node.js will use the CommonJS
module loader. See [Determining module system][] for more details.

<!-- Anchors to make sure old links find a target -->

<i id="esm_package_entry_points"></i><i id="esm_main_entry_point_export"></i><i id="esm_subpath_exports"></i><i id="esm_package_exports_fallbacks"></i><i id="esm_exports_sugar"></i><i id="esm_conditional_exports"></i><i id="esm_nested_conditions"></i><i id="esm_self_referencing_a_package_using_its_name"></i><i id="esm_internal_package_imports"></i><i id="esm_dual_commonjs_es_module_packages"></i><i id="esm_dual_package_hazard"></i><i id="esm_writing_dual_packages_while_avoiding_or_minimizing_hazards"></i><i id="esm_approach_1_use_an_es_module_wrapper"></i><i id="esm_approach_2_isolate_state"></i>

## Packages

This section was moved to [Modules: Packages](packages.md).

## `import` Specifiers

### Terminology

The _specifier_ of an `import` statement is the string after the `from` keyword,
e.g. `'path'` in `import { sep } from 'path'`. Specifiers are also used in
`export from` statements, and as the argument to an `import()` expression.

There are three types of specifiers:

* _Relative specifiers_ like `'./startup.js'` or `'../config.mjs'`. They refer
  to a path relative to the location of the importing file. _The file extension
  is always necessary for these._

* _Bare specifiers_ like `'some-package'` or `'some-package/shuffle'`. They can
  refer to the main entry point of a package by the package name, or a
  specific feature module within a package prefixed by the package name as per
  the examples respectively. _Including the file extension is only necessary
  for packages without an [`"exports"`][] field._

* _Absolute specifiers_ like `'file:///opt/nodejs/config.js'`. They refer
  directly and explicitly to a full path.

Bare specifier resolutions are handled by the [Node.js module resolution
algorithm][]. All other specifier resolutions are always only resolved with
the standard relative [URL][] resolution semantics.

Like in CommonJS, module files within packages can be accessed by appending a
path to the package name unless the package’s [`package.json`][] contains an
[`"exports"`][] field, in which case files within packages can only be accessed
via the paths defined in [`"exports"`][].

For details on these package resolution rules that apply to bare specifiers in
the Node.js module resolution, see the [packages documentation](packages.md).

### Mandatory file extensions

A file extension must be provided when using the `import` keyword to resolve
relative or absolute specifiers. Directory indexes (e.g. `'./startup/index.js'`)
must also be fully specified.

This behavior matches how `import` behaves in browser environments, assuming a
typically configured server.

### URLs

ES modules are resolved and cached as URLs. This means that special characters
must be [percent-encoded][], such as `#` with `%23` and `?` with `%3F`.

`file:`, `node:`, and `data:` URL schemes are supported. A specifier like
`'https://example.com/app.js'` is not supported natively in Node.js unless using
a [custom HTTPS loader][].

#### `file:` URLs

Modules are loaded multiple times if the `import` specifier used to resolve
them has a different query or fragment.

```js
import './foo.mjs?query=1'; // loads ./foo.mjs with query of "?query=1"
import './foo.mjs?query=2'; // loads ./foo.mjs with query of "?query=2"
```

The volume root may be referenced via `/`, `//` or `file:///`. Given the
differences between [URL][] and path resolution (such as percent encoding
details), it is recommended to use [url.pathToFileURL][] when importing a path.

#### `data:` Imports

<!-- YAML
added: v12.10.0
-->

[`data:` URLs][] are supported for importing with the following MIME types:

* `text/javascript` for ES Modules
* `application/json` for JSON
* `application/wasm` for Wasm

`data:` URLs only resolve [_Bare specifiers_][Terminology] for builtin modules
and [_Absolute specifiers_][Terminology]. Resolving
[_Relative specifiers_][Terminology] does not work because `data:` is not a
[special scheme][]. For example, attempting to load `./foo`
from `data:text/javascript,import "./foo";` fails to resolve because there
is no concept of relative resolution for `data:` URLs. An example of a `data:`
URLs being used is:

```js
import 'data:text/javascript,console.log("hello!");';
import _ from 'data:application/json,"world!"';
```

#### `node:` Imports

<!-- YAML
added:
  - v14.13.1
  - v12.20.0
changes:
  - version: v16.0.0
    pr-url: https://github.com/nodejs/node/pull/37246
    description: Added `node:` import support to `require(...)`.
-->

`node:` URLs are supported as an alternative means to load Node.js builtin
modules. This URL scheme allows for builtin modules to be referenced by valid
absolute URL strings.

```js
import fs from 'node:fs/promises';
```

## Import assertions

<!-- YAML
added: v16.14.0
-->

> Stability: 1 - Experimental

The [Import Assertions proposal][] adds an inline syntax for module import
statements to pass on more information alongside the module specifier.

```js
import fooData from './foo.json' assert { type: 'json' };

const { default: barData } =
  await import('./bar.json', { assert: { type: 'json' } });
```

Node.js supports the following `type` values, for which the assertion is
mandatory:

| Assertion `type` | Needed for       |
| ---------------- | ---------------- |
| `'json'`         | [JSON modules][] |

## Builtin modules

[Core modules][] provide named exports of their public API. A
default export is also provided which is the value of the CommonJS exports.
The default export can be used for, among other things, modifying the named
exports. Named exports of builtin modules are updated only by calling
[`module.syncBuiltinESMExports()`][].

```js
import EventEmitter from 'events';
const e = new EventEmitter();
```

```js
import { readFile } from 'fs';
readFile('./foo.txt', (err, source) => {
  if (err) {
    console.error(err);
  } else {
    console.log(source);
  }
});
```

```js
import fs, { readFileSync } from 'fs';
import { syncBuiltinESMExports } from 'module';
import { Buffer } from 'buffer';

fs.readFileSync = () => Buffer.from('Hello, ESM');
syncBuiltinESMExports();

fs.readFileSync === readFileSync;
```

## `import()` expressions

[Dynamic `import()`][] is supported in both CommonJS and ES modules. In CommonJS
modules it can be used to load ES modules.

## `import.meta`

* {Object}

The `import.meta` meta property is an `Object` that contains the following
properties.

### `import.meta.url`

* {string} The absolute `file:` URL of the module.

This is defined exactly the same as it is in browsers providing the URL of the
current module file.

This enables useful patterns such as relative file loading:

```js
import { readFileSync } from 'fs';
const buffer = readFileSync(new URL('./data.proto', import.meta.url));
```

### `import.meta.resolve(specifier[, parent])`

<!--
added:
  - v13.9.0
  - v12.16.2
changes:
  - version: v16.2.0
    pr-url: https://github.com/nodejs/node/pull/38587
    description: Add support for WHATWG `URL` object to `parentURL` parameter.
-->

> Stability: 1 - Experimental

This feature is only available with the `--experimental-import-meta-resolve`
command flag enabled.

* `specifier` {string} The module specifier to resolve relative to `parent`.
* `parent` {string|URL} The absolute parent module URL to resolve from. If none
  is specified, the value of `import.meta.url` is used as the default.
* Returns: {Promise}

Provides a module-relative resolution function scoped to each module, returning
the URL string.

<!-- eslint-skip -->

```js
const dependencyAsset = await import.meta.resolve('component-lib/asset.css');
```

`import.meta.resolve` also accepts a second argument which is the parent module
from which to resolve from:

<!-- eslint-skip -->

```js
await import.meta.resolve('./dep', import.meta.url);
```

This function is asynchronous because the ES module resolver in Node.js is
allowed to be asynchronous.

## Interoperability with CommonJS

### `import` statements

An `import` statement can reference an ES module or a CommonJS module.
`import` statements are permitted only in ES modules, but dynamic [`import()`][]
expressions are supported in CommonJS for loading ES modules.

When importing [CommonJS modules](#commonjs-namespaces), the
`module.exports` object is provided as the default export. Named exports may be
available, provided by static analysis as a convenience for better ecosystem
compatibility.

### `require`

The CommonJS module `require` always treats the files it references as CommonJS.

Using `require` to load an ES module is not supported because ES modules have
asynchronous execution. Instead, use [`import()`][] to load an ES module
from a CommonJS module.

### CommonJS Namespaces

CommonJS modules consist of a `module.exports` object which can be of any type.

When importing a CommonJS module, it can be reliably imported using the ES
module default import or its corresponding sugar syntax:

<!-- eslint-disable no-duplicate-imports -->

```js
import { default as cjs } from 'cjs';

// The following import statement is "syntax sugar" (equivalent but sweeter)
// for `{ default as cjsSugar }` in the above import statement:
import cjsSugar from 'cjs';

console.log(cjs);
console.log(cjs === cjsSugar);
// Prints:
//   <module.exports>
//   true
```

The ECMAScript Module Namespace representation of a CommonJS module is always
a namespace with a `default` export key pointing to the CommonJS
`module.exports` value.

This Module Namespace Exotic Object can be directly observed either when using
`import * as m from 'cjs'` or a dynamic import:

<!-- eslint-skip -->

```js
import * as m from 'cjs';
console.log(m);
console.log(m === await import('cjs'));
// Prints:
//   [Module] { default: <module.exports> }
//   true
```

For better compatibility with existing usage in the JS ecosystem, Node.js
in addition attempts to determine the CommonJS named exports of every imported
CommonJS module to provide them as separate ES module exports using a static
analysis process.

For example, consider a CommonJS module written:

```cjs
// cjs.cjs
exports.name = 'exported';
```

The preceding module supports named imports in ES modules:

<!-- eslint-disable no-duplicate-imports -->

```js
import { name } from './cjs.cjs';
console.log(name);
// Prints: 'exported'

import cjs from './cjs.cjs';
console.log(cjs);
// Prints: { name: 'exported' }

import * as m from './cjs.cjs';
console.log(m);
// Prints: [Module] { default: { name: 'exported' }, name: 'exported' }
```

As can be seen from the last example of the Module Namespace Exotic Object being
logged, the `name` export is copied off of the `module.exports` object and set
directly on the ES module namespace when the module is imported.

Live binding updates or new exports added to `module.exports` are not detected
for these named exports.

The detection of named exports is based on common syntax patterns but does not
always correctly detect named exports. In these cases, using the default
import form described above can be a better option.

Named exports detection covers many common export patterns, reexport patterns
and build tool and transpiler outputs. See [cjs-module-lexer][] for the exact
semantics implemented.

### Differences between ES modules and CommonJS

#### No `require`, `exports` or `module.exports`

In most cases, the ES module `import` can be used to load CommonJS modules.

If needed, a `require` function can be constructed within an ES module using
[`module.createRequire()`][].

#### No `__filename` or `__dirname`

These CommonJS variables are not available in ES modules.

`__filename` and `__dirname` use cases can be replicated via
[`import.meta.url`][].

#### No Native Module Loading

Native modules are not currently supported with ES module imports.

They can instead be loaded with [`module.createRequire()`][] or
[`process.dlopen`][].

#### No `require.resolve`

Relative resolution can be handled via `new URL('./local', import.meta.url)`.

For a complete `require.resolve` replacement, there is a flagged experimental
[`import.meta.resolve`][] API.

Alternatively `module.createRequire()` can be used.

#### No `NODE_PATH`

`NODE_PATH` is not part of resolving `import` specifiers. Please use symlinks
if this behavior is desired.

#### No `require.extensions`

`require.extensions` is not used by `import`. The expectation is that loader
hooks can provide this workflow in the future.

#### No `require.cache`

`require.cache` is not used by `import` as the ES module loader has its own
separate cache.

<i id="esm_experimental_json_modules"></i>

## JSON modules

> Stability: 1 - Experimental

JSON files can be referenced by `import`:

```js
import packageConfig from './package.json' assert { type: 'json' };
```

The `assert { type: 'json' }` syntax is mandatory; see [Import Assertions][].

The imported JSON only exposes a `default` export. There is no support for named
exports. A cache entry is created in the CommonJS cache to avoid duplication.
The same object is returned in CommonJS if the JSON module has already been
imported from the same path.

<i id="esm_experimental_wasm_modules"></i>

## Wasm modules

> Stability: 1 - Experimental

Importing WebAssembly modules is supported under the
`--experimental-wasm-modules` flag, allowing any `.wasm` files to be
imported as normal modules while also supporting their module imports.

This integration is in line with the
[ES Module Integration Proposal for WebAssembly][].

For example, an `index.mjs` containing:

```js
import * as M from './module.wasm';
console.log(M);
```

executed under:

```bash
node --experimental-wasm-modules index.mjs
```

would provide the exports interface for the instantiation of `module.wasm`.

<i id="esm_experimental_top_level_await"></i>

## Top-level `await`

<!-- YAML
added: v14.8.0
-->

> Stability: 1 - Experimental

The `await` keyword may be used in the top level body of an ECMAScript module.

Assuming an `a.mjs` with

```js
export const five = await Promise.resolve(5);
```

And a `b.mjs` with

```js
import { five } from './a.mjs';

console.log(five); // Logs `5`
```

```bash
node b.mjs # works
```

If a top level `await` expression never resolves, the `node` process will exit
with a `13` [status code][].

```js
import { spawn } from 'child_process';
import { execPath } from 'process';

spawn(execPath, [
  '--input-type=module',
  '--eval',
  // Never-resolving Promise:
  'await new Promise(() => {})',
]).once('exit', (code) => {
  console.log(code); // Logs `13`
});
```

## HTTPS and HTTP imports

> Stability: 1 - Experimental

Importing network based modules using `https:` and `http:` is supported under
the `--experimental-network-imports` flag. This allows web browser-like imports
to work in Node.js with a few differences due to application stability and
security concerns that are different when running in a privileged environment
instead of a browser sandbox.

### Imports are limited to HTTP/1

Automatic protocol negotiation for HTTP/2 and HTTP/3 is not yet supported.

### HTTP is limited to loopback addresses

`http:` is vulnerable to man-in-the-middle attacks and is not allowed to be
used for addresses outside of the IPv4 address `127.0.0.0/8` (`127.0.0.1` to
`127.255.255.255`) and the IPv6 address `::1`. Support for `http:` is intended
to be used for local development.

### Authentication is never sent to the destination server.

`Authorization`, `Cookie`, and `Proxy-Authorization` headers are not sent to the
server. Avoid including user info in parts of imported URLs. A security model
for safely using these on the server is being worked on.

### CORS is never checked on the destination server

CORS is designed to allow a server to limit the consumers of an API to a
specific set of hosts. This is not supported as it does not make sense for a
server-based implementation.

### Cannot load non-network dependencies

These modules cannot access other modules that are not over `http:` or `https:`.
To still access local modules while avoiding the security concern, pass in
references to the local dependencies:

```mjs
// file.mjs
import worker_threads from 'worker_threads';
import { configure, resize } from 'https://example.com/imagelib.mjs';
configure({ worker_threads });
```

```mjs
// https://example.com/imagelib.mjs
let worker_threads;
export function configure(opts) {
  worker_threads = opts.worker_threads;
}
export function resize(img, size) {
  // Perform resizing in worker_thread to avoid main thread blocking
}
```

### Network-based loading is not enabled by default

For now, the `--experimental-network-imports` flag is required to enable loading
resources over `http:` or `https:`. In the future, a different mechanism will be
used to enforce this. Opt-in is required to prevent transitive dependencies
inadvertently using potentially mutable state that could affect reliability
of Node.js applications.

<i id="esm_experimental_loaders"></i>

## Loaders

> Stability: 1 - Experimental

**Note: This API is currently being redesigned and will still change.**

<!-- type=misc -->

To customize the default module resolution, loader hooks can optionally be
provided via a `--experimental-loader ./loader-name.mjs` argument to Node.js.

When hooks are used they apply to the entry point and all `import` calls. They
won't apply to `require` calls; those still follow [CommonJS][] rules.

### Hooks

#### `resolve(specifier, context, defaultResolve)`

<!-- YAML
changes:
  - version: v16.14.0
    pr-url: https://github.com/nodejs/node/pull/40250
    description: Add support for import assertions.
-->

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

* `specifier` {string}
* `context` {Object}
  * `conditions` {string\[]}
  * `importAssertions` {Object}
  * `parentURL` {string|undefined}
* `defaultResolve` {Function} The Node.js default resolver.
* Returns: {Object}
  * `format` {string|null|undefined}
    `'builtin' | 'commonjs' | 'json' | 'module' | 'wasm'`
  * `url` {string} The absolute url to the import target (such as `file://…`)

The `resolve` hook returns the resolved file URL for a given module specifier
and parent URL, and optionally its format (such as `'module'`) as a hint to the
`load` hook. If a format is specified, the `load` hook is ultimately responsible
for providing the final `format` value (and it is free to ignore the hint
provided by `resolve`); if `resolve` provides a `format`, a custom `load`
hook is required even if only to pass the value to the Node.js default `load`
hook.

The module specifier is the string in an `import` statement or
`import()` expression, and the parent URL is the URL of the module that imported
this one, or `undefined` if this is the main entry point for the application.

The `conditions` property in `context` is an array of conditions for
[package exports conditions][Conditional Exports] that apply to this resolution
request. They can be used for looking up conditional mappings elsewhere or to
modify the list when calling the default resolution logic.

The current [package exports conditions][Conditional Exports] are always in
the `context.conditions` array passed into the hook. To guarantee _default
Node.js module specifier resolution behavior_ when calling `defaultResolve`, the
`context.conditions` array passed to it _must_ include _all_ elements of the
`context.conditions` array originally passed into the `resolve` hook.

```js
/**
 * @param {string} specifier
 * @param {{
 *   conditions: string[],
 *   parentURL: string | undefined,
 * }} context
 * @param {Function} defaultResolve
 * @returns {Promise<{ url: string }>}
 */
export async function resolve(specifier, context, defaultResolve) {
  const { parentURL = null } = context;
  if (Math.random() > 0.5) { // Some condition.
    // For some or all specifiers, do some custom logic for resolving.
    // Always return an object of the form {url: <string>}.
    return {
      url: parentURL ?
        new URL(specifier, parentURL).href :
        new URL(specifier).href,
    };
  }
  if (Math.random() < 0.5) { // Another condition.
    // When calling `defaultResolve`, the arguments can be modified. In this
    // case it's adding another value for matching conditional exports.
    return defaultResolve(specifier, {
      ...context,
      conditions: [...context.conditions, 'another-condition'],
    });
  }
  // Defer to Node.js for all other specifiers.
  return defaultResolve(specifier, context, defaultResolve);
}
```

#### `load(url, context, defaultLoad)`

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

> Note: In a previous version of this API, this was split across 3 separate, now
> deprecated, hooks (`getFormat`, `getSource`, and `transformSource`).

* `url` {string}
* `context` {Object}
  * `format` {string|null|undefined} The format optionally supplied by the
    `resolve` hook.
  * `importAssertions` {Object}
* `defaultLoad` {Function}
* Returns: {Object}
  * `format` {string}
  * `source` {string|ArrayBuffer|TypedArray}

The `load` hook provides a way to define a custom method of determining how
a URL should be interpreted, retrieved, and parsed. It is also in charge of
validating the import assertion.

The final value of `format` must be one of the following:

| `format`     | Description                    | Acceptable types for `source` returned by `load`      |
| ------------ | ------------------------------ | ----------------------------------------------------- |
| `'builtin'`  | Load a Node.js builtin module  | Not applicable                                        |
| `'commonjs'` | Load a Node.js CommonJS module | Not applicable                                        |
| `'json'`     | Load a JSON file               | { [`string`][], [`ArrayBuffer`][], [`TypedArray`][] } |
| `'module'`   | Load an ES module              | { [`string`][], [`ArrayBuffer`][], [`TypedArray`][] } |
| `'wasm'`     | Load a WebAssembly module      | { [`ArrayBuffer`][], [`TypedArray`][] }               |

The value of `source` is ignored for type `'builtin'` because currently it is
not possible to replace the value of a Node.js builtin (core) module. The value
of `source` is ignored for type `'commonjs'` because the CommonJS module loader
does not provide a mechanism for the ES module loader to override the
[CommonJS module return value](#commonjs-namespaces). This limitation might be
overcome in the future.

> **Caveat**: The ESM `load` hook and namespaced exports from CommonJS modules
> are incompatible. Attempting to use them together will result in an empty
> object from the import. This may be addressed in the future.

> Note: These types all correspond to classes defined in ECMAScript.

* The specific [`ArrayBuffer`][] object is a [`SharedArrayBuffer`][].
* The specific [`TypedArray`][] object is a [`Uint8Array`][].

If the source value of a text-based format (i.e., `'json'`, `'module'`)
is not a string, it is converted to a string using [`util.TextDecoder`][].

The `load` hook provides a way to define a custom method for retrieving the
source code of an ES module specifier. This would allow a loader to potentially
avoid reading files from disk. It could also be used to map an unrecognized
format to a supported one, for example `yaml` to `module`.

```js
/**
 * @param {string} url
 * @param {{
    format: string,
  }} context If resolve settled with a `format`, that value is included here.
 * @param {Function} defaultLoad
 * @returns {Promise<{
    format: string,
    source: string | ArrayBuffer | SharedArrayBuffer | Uint8Array,
  }>}
 */
export async function load(url, context, defaultLoad) {
  const { format } = context;
  if (Math.random() > 0.5) { // Some condition.
    /*
      For some or all URLs, do some custom logic for retrieving the source.
      Always return an object of the form {
        format: <string>,
        source: <string|buffer>,
      }.
    */
    return {
      format,
      source: '...',
    };
  }
  // Defer to Node.js for all other URLs.
  return defaultLoad(url, context, defaultLoad);
}
```

In a more advanced scenario, this can also be used to transform an unsupported
source to a supported one (see [Examples](#examples) below).

#### `globalPreload()`

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

> Note: In a previous version of this API, this hook was named
> `getGlobalPreloadCode`.

* Returns: {string}

Sometimes it might be necessary to run some code inside of the same global
scope that the application runs in. This hook allows the return of a string
that is run as a sloppy-mode script on startup.

Similar to how CommonJS wrappers work, the code runs in an implicit function
scope. The only argument is a `require`-like function that can be used to load
builtins like "fs": `getBuiltin(request: string)`.

If the code needs more advanced `require` features, it has to construct
its own `require` using  `module.createRequire()`.

```js
/**
 * @param {{
     port: MessagePort,
   }} utilities Things that preload code might find useful
 * @returns {string} Code to run before application startup
 */
export function globalPreload(utilities) {
  return `\
globalThis.someInjectedProperty = 42;
console.log('I just set some globals!');

const { createRequire } = getBuiltin('module');
const { cwd } = getBuiltin('process');

const require = createRequire(cwd() + '/<preload>');
// [...]
`;
}
```

In order to allow communication between the application and the loader, another
argument is provided to the preload code: `port`. This is available as a
parameter to the loader hook and inside of the source text returned by the hook.
Some care must be taken in order to properly call [`port.ref()`][] and
[`port.unref()`][] to prevent a process from being in a state where it won't
close normally.

```js
/**
 * This example has the application context send a message to the loader
 * and sends the message back to the application context
 * @param {{
     port: MessagePort,
   }} utilities Things that preload code might find useful
 * @returns {string} Code to run before application startup
 */
export function globalPreload({ port }) {
  port.onmessage = (evt) => {
    port.postMessage(evt.data);
  };
  return `\
    port.postMessage('console.log("I went to the Loader and back");');
    port.onmessage = (evt) => {
      eval(evt.data);
    };
  `;
}
```

### Examples

The various loader hooks can be used together to accomplish wide-ranging
customizations of Node.js’ code loading and evaluation behaviors.

#### HTTPS loader

In current Node.js, specifiers starting with `https://` are unsupported. The
loader below registers hooks to enable rudimentary support for such specifiers.
While this may seem like a significant improvement to Node.js core
functionality, there are substantial downsides to actually using this loader:
performance is much slower than loading files from disk, there is no caching,
and there is no security.

```js
// https-loader.mjs
import { get } from 'https';

export function resolve(specifier, context, defaultResolve) {
  const { parentURL = null } = context;

  // Normally Node.js would error on specifiers starting with 'https://', so
  // this hook intercepts them and converts them into absolute URLs to be
  // passed along to the later hooks below.
  if (specifier.startsWith('https://')) {
    return {
      url: specifier
    };
  } else if (parentURL && parentURL.startsWith('https://')) {
    return {
      url: new URL(specifier, parentURL).href
    };
  }

  // Let Node.js handle all other specifiers.
  return defaultResolve(specifier, context, defaultResolve);
}

export function load(url, context, defaultLoad) {
  // For JavaScript to be loaded over the network, we need to fetch and
  // return it.
  if (url.startsWith('https://')) {
    return new Promise((resolve, reject) => {
      get(url, (res) => {
        let data = '';
        res.on('data', (chunk) => data += chunk);
        res.on('end', () => resolve({
          // This example assumes all network-provided JavaScript is ES module
          // code.
          format: 'module',
          source: data,
        }));
      }).on('error', (err) => reject(err));
    });
  }

  // Let Node.js handle all other URLs.
  return defaultLoad(url, context, defaultLoad);
}
```

```js
// main.mjs
import { VERSION } from 'https://coffeescript.org/browser-compiler-modern/coffeescript.js';

console.log(VERSION);
```

With the preceding loader, running
`node --experimental-loader ./https-loader.mjs ./main.mjs`
prints the current version of CoffeeScript per the module at the URL in
`main.mjs`.

#### Transpiler loader

Sources that are in formats Node.js doesn’t understand can be converted into
JavaScript using the [`load` hook][load hook]. Before that hook gets called,
however, a [`resolve` hook][resolve hook] needs to tell Node.js not to
throw an error on unknown file types.

This is less performant than transpiling source files before running
Node.js; a transpiler loader should only be used for development and testing
purposes.

```js
// coffeescript-loader.mjs
import { readFile } from 'node:fs/promises';
import { dirname, extname, resolve as resolvePath } from 'node:path';
import { cwd } from 'node:process';
import { fileURLToPath, pathToFileURL } from 'node:url';
import CoffeeScript from 'coffeescript';

const baseURL = pathToFileURL(`${cwd()}/`).href;

// CoffeeScript files end in .coffee, .litcoffee or .coffee.md.
const extensionsRegex = /\.coffee$|\.litcoffee$|\.coffee\.md$/;

export async function resolve(specifier, context, defaultResolve) {
  const { parentURL = baseURL } = context;

  // Node.js normally errors on unknown file extensions, so return a URL for
  // specifiers ending in the CoffeeScript file extensions.
  if (extensionsRegex.test(specifier)) {
    return {
      url: new URL(specifier, parentURL).href
    };
  }

  // Let Node.js handle all other specifiers.
  return defaultResolve(specifier, context, defaultResolve);
}

export async function load(url, context, defaultLoad) {
  // Now that we patched resolve to let CoffeeScript URLs through, we need to
  // tell Node.js what format such URLs should be interpreted as. Because
  // CoffeeScript transpiles into JavaScript, it should be one of the two
  // JavaScript formats: 'commonjs' or 'module'.
  if (extensionsRegex.test(url)) {
    // CoffeeScript files can be either CommonJS or ES modules, so we want any
    // CoffeeScript file to be treated by Node.js the same as a .js file at the
    // same location. To determine how Node.js would interpret an arbitrary .js
    // file, search up the file system for the nearest parent package.json file
    // and read its "type" field.
    const format = await getPackageType(url);
    // When a hook returns a format of 'commonjs', `source` is be ignored.
    // To handle CommonJS files, a handler needs to be registered with
    // `require.extensions` in order to process the files with the CommonJS
    // loader. Avoiding the need for a separate CommonJS handler is a future
    // enhancement planned for ES module loaders.
    if (format === 'commonjs') {
      return { format };
    }

    const { source: rawSource } = await defaultLoad(url, { format });
    // This hook converts CoffeeScript source code into JavaScript source code
    // for all imported CoffeeScript files.
    const transformedSource = CoffeeScript.compile(rawSource.toString(), {
      bare: true,
      filename: url,
    });

    return {
      format,
      source: transformedSource,
    };
  }

  // Let Node.js handle all other URLs.
  return defaultLoad(url, context, defaultLoad);
}

async function getPackageType(url) {
  // `url` is only a file path during the first iteration when passed the
  // resolved url from the load() hook
  // an actual file path from load() will contain a file extension as it's
  // required by the spec
  // this simple truthy check for whether `url` contains a file extension will
  // work for most projects but does not cover some edge-cases (such as
  // extension-less files or a url ending in a trailing space)
  const isFilePath = !!extname(url);
  // If it is a file path, get the directory it's in
  const dir = isFilePath ?
    dirname(fileURLToPath(url)) :
    url;
  // Compose a file path to a package.json in the same directory,
  // which may or may not exist
  const packagePath = resolvePath(dir, 'package.json');
  // Try to read the possibly nonexistent package.json
  const type = await readFile(packagePath, { encoding: 'utf8' })
    .then((filestring) => JSON.parse(filestring).type)
    .catch((err) => {
      if (err?.code !== 'ENOENT') console.error(err);
    });
  // Ff package.json existed and contained a `type` field with a value, voila
  if (type) return type;
  // Otherwise, (if not at the root) continue checking the next directory up
  // If at the root, stop and return false
  return dir.length > 1 && getPackageType(resolvePath(dir, '..'));
}
```

```coffee
# main.coffee
import { scream } from './scream.coffee'
console.log scream 'hello, world'

import { version } from 'process'
console.log "Brought to you by Node.js version #{version}"
```

```coffee
# scream.coffee
export scream = (str) -> str.toUpperCase()
```

With the preceding loader, running
`node --experimental-loader ./coffeescript-loader.mjs main.coffee`
causes `main.coffee` to be turned into JavaScript after its source code is
loaded from disk but before Node.js executes it; and so on for any `.coffee`,
`.litcoffee` or `.coffee.md` files referenced via `import` statements of any
loaded file.

## Resolution algorithm

### Features

The resolver has the following properties:

* FileURL-based resolution as is used by ES modules
* Support for builtin module loading
* Relative and absolute URL resolution
* No default extensions
* No folder mains
* Bare specifier package resolution lookup through node\_modules

### Resolver algorithm

The algorithm to load an ES module specifier is given through the
**ESM\_RESOLVE** method below. It returns the resolved URL for a
module specifier relative to a parentURL.

The algorithm to determine the module format of a resolved URL is
provided by **ESM\_FORMAT**, which returns the unique module
format for any file. The _"module"_ format is returned for an ECMAScript
Module, while the _"commonjs"_ format is used to indicate loading through the
legacy CommonJS loader. Additional formats such as _"addon"_ can be extended in
future updates.

In the following algorithms, all subroutine errors are propagated as errors
of these top-level routines unless stated otherwise.

_defaultConditions_ is the conditional environment name array,
`["node", "import"]`.

The resolver can throw the following errors:

* _Invalid Module Specifier_: Module specifier is an invalid URL, package name
  or package subpath specifier.
* _Invalid Package Configuration_: package.json configuration is invalid or
  contains an invalid configuration.
* _Invalid Package Target_: Package exports or imports define a target module
  for the package that is an invalid type or string target.
* _Package Path Not Exported_: Package exports do not define or permit a target
  subpath in the package for the given module.
* _Package Import Not Defined_: Package imports do not define the specifier.
* _Module Not Found_: The package or module requested does not exist.
* _Unsupported Directory Import_: The resolved path corresponds to a directory,
  which is not a supported target for module imports.

### Resolver Algorithm Specification

**ESM\_RESOLVE**(_specifier_, _parentURL_)

> 1. Let _resolved_ be **undefined**.
> 2. If _specifier_ is a valid URL, then
>    1. Set _resolved_ to the result of parsing and reserializing
>       _specifier_ as a URL.
> 3. Otherwise, if _specifier_ starts with _"/"_, _"./"_ or _"../"_, then
>    1. Set _resolved_ to the URL resolution of _specifier_ relative to
>       _parentURL_.
> 4. Otherwise, if _specifier_ starts with _"#"_, then
>    1. Set _resolved_ to the destructured value of the result of
>       **PACKAGE\_IMPORTS\_RESOLVE**(_specifier_, _parentURL_,
>       _defaultConditions_).
> 5. Otherwise,
>    1. Note: _specifier_ is now a bare specifier.
>    2. Set _resolved_ the result of
>       **PACKAGE\_RESOLVE**(_specifier_, _parentURL_).
> 6. Let _format_ be **undefined**.
> 7. If _resolved_ is a _"file:"_ URL, then
>    1. If _resolved_ contains any percent encodings of _"/"_ or _"\\"_ (_"%2F"_
>       and _"%5C"_ respectively), then
>       1. Throw an _Invalid Module Specifier_ error.
>    2. If the file at _resolved_ is a directory, then
>       1. Throw an _Unsupported Directory Import_ error.
>    3. If the file at _resolved_ does not exist, then
>       1. Throw a _Module Not Found_ error.
>    4. Set _resolved_ to the real path of _resolved_, maintaining the
>       same URL querystring and fragment components.
>    5. Set _format_ to the result of **ESM\_FILE\_FORMAT**(_resolved_).
> 8. Otherwise,
>    1. Set _format_ the module format of the content type associated with the
>       URL _resolved_.
> 9. Load _resolved_ as module format, _format_.

**PACKAGE\_RESOLVE**(_packageSpecifier_, _parentURL_)

> 1. Let _packageName_ be **undefined**.
> 2. If _packageSpecifier_ is an empty string, then
>    1. Throw an _Invalid Module Specifier_ error.
> 3. If _packageSpecifier_ is a Node.js builtin module name, then
>    1. Return the string _"node:"_ concatenated with _packageSpecifier_.
> 4. If _packageSpecifier_ does not start with _"@"_, then
>    1. Set _packageName_ to the substring of _packageSpecifier_ until the first
>       _"/"_ separator or the end of the string.
> 5. Otherwise,
>    1. If _packageSpecifier_ does not contain a _"/"_ separator, then
>       1. Throw an _Invalid Module Specifier_ error.
>    2. Set _packageName_ to the substring of _packageSpecifier_
>       until the second _"/"_ separator or the end of the string.
> 6. If _packageName_ starts with _"."_ or contains _"\\"_ or _"%"_, then
>    1. Throw an _Invalid Module Specifier_ error.
> 7. Let _packageSubpath_ be _"."_ concatenated with the substring of
>    _packageSpecifier_ from the position at the length of _packageName_.
> 8. If _packageSubpath_ ends in _"/"_, then
>    1. Throw an _Invalid Module Specifier_ error.
> 9. Let _selfUrl_ be the result of
>    **PACKAGE\_SELF\_RESOLVE**(_packageName_, _packageSubpath_, _parentURL_).
> 10. If _selfUrl_ is not **undefined**, return _selfUrl_.
> 11. While _parentURL_ is not the file system root,
>     1. Let _packageURL_ be the URL resolution of _"node\_modules/"_
>        concatenated with _packageSpecifier_, relative to _parentURL_.
>     2. Set _parentURL_ to the parent folder URL of _parentURL_.
>     3. If the folder at _packageURL_ does not exist, then
>        1. Continue the next loop iteration.
>     4. Let _pjson_ be the result of **READ\_PACKAGE\_JSON**(_packageURL_).
>     5. If _pjson_ is not **null** and _pjson_._exports_ is not **null** or
>        **undefined**, then
>        1. Return the result of **PACKAGE\_EXPORTS\_RESOLVE**(_packageURL_,
>           _packageSubpath_, _pjson.exports_, _defaultConditions_).
>     6. Otherwise, if _packageSubpath_ is equal to _"."_, then
>        1. If _pjson.main_ is a string, then
>           1. Return the URL resolution of _main_ in _packageURL_.
>     7. Otherwise,
>        1. Return the URL resolution of _packageSubpath_ in _packageURL_.
> 12. Throw a _Module Not Found_ error.

**PACKAGE\_SELF\_RESOLVE**(_packageName_, _packageSubpath_, _parentURL_)

> 1. Let _packageURL_ be the result of **LOOKUP\_PACKAGE\_SCOPE**(_parentURL_).
> 2. If _packageURL_ is **null**, then
>    1. Return **undefined**.
> 3. Let _pjson_ be the result of **READ\_PACKAGE\_JSON**(_packageURL_).
> 4. If _pjson_ is **null** or if _pjson_._exports_ is **null** or
>    **undefined**, then
>    1. Return **undefined**.
> 5. If _pjson.name_ is equal to _packageName_, then
>    1. Return the _resolved_ destructured value of the result of
>       **PACKAGE\_EXPORTS\_RESOLVE**(_packageURL_, _packageSubpath_,
>       _pjson.exports_, _defaultConditions_).
> 6. Otherwise, return **undefined**.

**PACKAGE\_EXPORTS\_RESOLVE**(_packageURL_, _subpath_, _exports_, _conditions_)

> 1. If _exports_ is an Object with both a key starting with _"."_ and a key not
>    starting with _"."_, throw an _Invalid Package Configuration_ error.
> 2. If _subpath_ is equal to _"."_, then
>    1. Let _mainExport_ be **undefined**.
>    2. If _exports_ is a String or Array, or an Object containing no keys
>       starting with _"."_, then
>       1. Set _mainExport_ to _exports_.
>    3. Otherwise if _exports_ is an Object containing a _"."_ property, then
>       1. Set _mainExport_ to _exports_\[_"."_].
>    4. If _mainExport_ is not **undefined**, then
>       1. Let _resolved_ be the result of **PACKAGE\_TARGET\_RESOLVE**(
>          _packageURL_, _mainExport_, _""_, **false**, **false**,
>          _conditions_).
>       2. If _resolved_ is not **null** or **undefined**, then
>          1. Return _resolved_.
> 3. Otherwise, if _exports_ is an Object and all keys of _exports_ start with
>    _"."_, then
>    1. Let _matchKey_ be the string _"./"_ concatenated with _subpath_.
>    2. Let _resolvedMatch_ be result of **PACKAGE\_IMPORTS\_EXPORTS\_RESOLVE**(
>       _matchKey_, _exports_, _packageURL_, **false**, _conditions_).
>    3. If _resolvedMatch_._resolve_ is not **null** or **undefined**, then
>       1. Return _resolvedMatch_.
> 4. Throw a _Package Path Not Exported_ error.

**PACKAGE\_IMPORTS\_RESOLVE**(_specifier_, _parentURL_, _conditions_)

> 1. Assert: _specifier_ begins with _"#"_.
> 2. If _specifier_ is exactly equal to _"#"_ or starts with _"#/"_, then
>    1. Throw an _Invalid Module Specifier_ error.
> 3. Let _packageURL_ be the result of **LOOKUP\_PACKAGE\_SCOPE**(_parentURL_).
> 4. If _packageURL_ is not **null**, then
>    1. Let _pjson_ be the result of **READ\_PACKAGE\_JSON**(_packageURL_).
>    2. If _pjson.imports_ is a non-null Object, then
>       1. Let _resolvedMatch_ be the result of
>          **PACKAGE\_IMPORTS\_EXPORTS\_RESOLVE**(_specifier_, _pjson.imports_,
>          _packageURL_, **true**, _conditions_).
>       2. If _resolvedMatch_._resolve_ is not **null** or **undefined**, then
>          1. Return _resolvedMatch_.
> 5. Throw a _Package Import Not Defined_ error.

**PACKAGE\_IMPORTS\_EXPORTS\_RESOLVE**(_matchKey_, _matchObj_, _packageURL_,
_isImports_, _conditions_)

> 1. If _matchKey_ is a key of _matchObj_ and does not end in _"/"_ or contain
>    _"\*"_, then
>    1. Let _target_ be the value of _matchObj_\[_matchKey_].
>    2. Let _resolved_ be the result of **PACKAGE\_TARGET\_RESOLVE**(
>       _packageURL_, _target_, _""_, **false**, _isImports_, _conditions_).
>    3. Return the object _{ resolved, exact: **true** }_.
> 2. Let _expansionKeys_ be the list of keys of _matchObj_ either ending in
>    _"/"_ or containing only a single _"\*"_, sorted by the sorting function
>    **PATTERN\_KEY\_COMPARE** which orders in descending order of specificity.
> 3. For each key _expansionKey_ in _expansionKeys_, do
>    1. Let _patternBase_ be **null**.
>    2. If _expansionKey_ contains _"\*"_, set _patternBase_ to the substring of
>       _expansionKey_ up to but excluding the first _"\*"_ character.
>    3. If _patternBase_ is not **null** and _matchKey_ starts with but is not
>       equal to _patternBase_, then
>       1. If _matchKey_ ends with _"/"_, throw an _Invalid Module Specifier_
>          error.
>       2. Let _patternTrailer_ be the substring of _expansionKey_ from the
>          index after the first _"\*"_ character.
>       3. If _patternTrailer_ has zero length, or if _matchKey_ ends with
>          _patternTrailer_ and the length of _matchKey_ is greater than or
>          equal to the length of _expansionKey_, then
>          1. Let _target_ be the value of _matchObj_\[_expansionKey_].
>          2. Let _subpath_ be the substring of _matchKey_ starting at the
>             index of the length of _patternBase_ up to the length of
>             _matchKey_ minus the length of _patternTrailer_.
>          3. Let _resolved_ be the result of **PACKAGE\_TARGET\_RESOLVE**(
>             _packageURL_, _target_, _subpath_, **true**, _isImports_,
>             _conditions_).
>          4. Return the object _{ resolved, exact: **true** }_.
>    4. Otherwise if _patternBase_ is **null** and _matchKey_ starts with
>       _expansionKey_, then
>       1. Let _target_ be the value of _matchObj_\[_expansionKey_].
>       2. Let _subpath_ be the substring of _matchKey_ starting at the
>          index of the length of _expansionKey_.
>       3. Let _resolved_ be the result of **PACKAGE\_TARGET\_RESOLVE**(
>          _packageURL_, _target_, _subpath_, **false**, _isImports_,
>          _conditions_).
>       4. Return the object _{ resolved, exact: **false** }_.
> 4. Return the object _{ resolved: **null**, exact: **true** }_.

**PATTERN\_KEY\_COMPARE**(_keyA_, _keyB_)

> 1. Assert: _keyA_ ends with _"/"_ or contains only a single _"\*"_.
> 2. Assert: _keyB_ ends with _"/"_ or contains only a single _"\*"_.
> 3. Let _baseLengthA_ be the index of _"\*"_ in _keyA_ plus one, if _keyA_
>    contains _"\*"_, or the length of _keyA_ otherwise.
> 4. Let _baseLengthB_ be the index of _"\*"_ in _keyB_ plus one, if _keyB_
>    contains _"\*"_, or the length of _keyB_ otherwise.
> 5. If _baseLengthA_ is greater than _baseLengthB_, return -1.
> 6. If _baseLengthB_ is greater than _baseLengthA_, return 1.
> 7. If _keyA_ does not contain _"\*"_, return 1.
> 8. If _keyB_ does not contain _"\*"_, return -1.
> 9. If the length of _keyA_ is greater than the length of _keyB_, return -1.
> 10. If the length of _keyB_ is greater than the length of _keyA_, return 1.
> 11. Return 0.

**PACKAGE\_TARGET\_RESOLVE**(_packageURL_, _target_, _subpath_, _pattern_,
_internal_, _conditions_)

> 1. If _target_ is a String, then
>    1. If _pattern_ is **false**, _subpath_ has non-zero length and _target_
>       does not end with _"/"_, throw an _Invalid Module Specifier_ error.
>    2. If _target_ does not start with _"./"_, then
>       1. If _internal_ is **true** and _target_ does not start with _"../"_ or
>          _"/"_ and is not a valid URL, then
>          1. If _pattern_ is **true**, then
>             1. Return **PACKAGE\_RESOLVE**(_target_ with every instance of
>                _"\*"_ replaced by _subpath_, _packageURL_ + _"/"_).
>          2. Return **PACKAGE\_RESOLVE**(_target_ + _subpath_,
>             _packageURL_ + _"/"_).
>       2. Otherwise, throw an _Invalid Package Target_ error.
>    3. If _target_ split on _"/"_ or _"\\"_ contains any _"."_, _".."_ or
>       _"node\_modules"_ segments after the first segment, case insensitive and
>       including percent encoded variants, throw an _Invalid Package Target_
>       error.
>    4. Let _resolvedTarget_ be the URL resolution of the concatenation of
>       _packageURL_ and _target_.
>    5. Assert: _resolvedTarget_ is contained in _packageURL_.
>    6. If _subpath_ split on _"/"_ or _"\\"_ contains any _"."_, _".."_ or
>       _"node\_modules"_ segments, case insensitive and including percent
>       encoded variants, throw an _Invalid Module Specifier_ error.
>    7. If _pattern_ is **true**, then
>       1. Return the URL resolution of _resolvedTarget_ with every instance of
>          _"\*"_ replaced with _subpath_.
>    8. Otherwise,
>       1. Return the URL resolution of the concatenation of _subpath_ and
>          _resolvedTarget_.
> 2. Otherwise, if _target_ is a non-null Object, then
>    1. If _exports_ contains any index property keys, as defined in ECMA-262
>       [6.1.7 Array Index][], throw an _Invalid Package Configuration_ error.
>    2. For each property _p_ of _target_, in object insertion order as,
>       1. If _p_ equals _"default"_ or _conditions_ contains an entry for _p_,
>          then
>          1. Let _targetValue_ be the value of the _p_ property in _target_.
>          2. Let _resolved_ be the result of **PACKAGE\_TARGET\_RESOLVE**(
>             _packageURL_, _targetValue_, _subpath_, _pattern_, _internal_,
>             _conditions_).
>          3. If _resolved_ is equal to **undefined**, continue the loop.
>          4. Return _resolved_.
>    3. Return **undefined**.
> 3. Otherwise, if _target_ is an Array, then
>    1. If \_target.length is zero, return **null**.
>    2. For each item _targetValue_ in _target_, do
>       1. Let _resolved_ be the result of **PACKAGE\_TARGET\_RESOLVE**(
>          _packageURL_, _targetValue_, _subpath_, _pattern_, _internal_,
>          _conditions_), continuing the loop on any _Invalid Package Target_
>          error.
>       2. If _resolved_ is **undefined**, continue the loop.
>       3. Return _resolved_.
>    3. Return or throw the last fallback resolution **null** return or error.
> 4. Otherwise, if _target_ is _null_, return **null**.
> 5. Otherwise throw an _Invalid Package Target_ error.

**ESM\_FILE\_FORMAT**(_url_)

> 1. Assert: _url_ corresponds to an existing file.
> 2. If _url_ ends in _".mjs"_, then
>    1. Return _"module"_.
> 3. If _url_ ends in _".cjs"_, then
>    1. Return _"commonjs"_.
> 4. If _url_ ends in _".json"_, then
>    1. Return _"json"_.
> 5. Let _packageURL_ be the result of **LOOKUP\_PACKAGE\_SCOPE**(_url_).
> 6. Let _pjson_ be the result of **READ\_PACKAGE\_JSON**(_packageURL_).
> 7. If _pjson?.type_ exists and is _"module"_, then
>    1. If _url_ ends in _".js"_, then
>       1. Return _"module"_.
>    2. Throw an _Unsupported File Extension_ error.
> 8. Otherwise,
>    1. Throw an _Unsupported File Extension_ error.

**LOOKUP\_PACKAGE\_SCOPE**(_url_)

> 1. Let _scopeURL_ be _url_.
> 2. While _scopeURL_ is not the file system root,
>    1. Set _scopeURL_ to the parent URL of _scopeURL_.
>    2. If _scopeURL_ ends in a _"node\_modules"_ path segment, return **null**.
>    3. Let _pjsonURL_ be the resolution of _"package.json"_ within
>       _scopeURL_.
>    4. if the file at _pjsonURL_ exists, then
>       1. Return _scopeURL_.
> 3. Return **null**.

**READ\_PACKAGE\_JSON**(_packageURL_)

> 1. Let _pjsonURL_ be the resolution of _"package.json"_ within _packageURL_.
> 2. If the file at _pjsonURL_ does not exist, then
>    1. Return **null**.
> 3. If the file at _packageURL_ does not parse as valid JSON, then
>    1. Throw an _Invalid Package Configuration_ error.
> 4. Return the parsed JSON source of the file at _pjsonURL_.

### Customizing ESM specifier resolution algorithm

> Stability: 1 - Experimental

The current specifier resolution does not support all default behavior of
the CommonJS loader. One of the behavior differences is automatic resolution
of file extensions and the ability to import directories that have an index
file.

The `--experimental-specifier-resolution=[mode]` flag can be used to customize
the extension resolution algorithm. The default mode is `explicit`, which
requires the full path to a module be provided to the loader. To enable the
automatic extension resolution and importing from directories that include an
index file use the `node` mode.

```console
$ node index.mjs
success!
$ node index # Failure!
Error: Cannot find module
$ node --experimental-specifier-resolution=node index
success!
```

<!-- Note: The cjs-module-lexer link should be kept in-sync with the deps version -->

[6.1.7 Array Index]: https://tc39.es/ecma262/#integer-index
[CommonJS]: modules.md
[Conditional exports]: packages.md#conditional-exports
[Core modules]: modules.md#core-modules
[Determining module system]: packages.md#determining-module-system
[Dynamic `import()`]: https://wiki.developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import#Dynamic_Imports
[ES Module Integration Proposal for WebAssembly]: https://github.com/webassembly/esm-integration
[Import Assertions]: #import-assertions
[Import Assertions proposal]: https://github.com/tc39/proposal-import-assertions
[JSON modules]: #json-modules
[Node.js Module Resolution Algorithm]: #resolver-algorithm-specification
[Terminology]: #terminology
[URL]: https://url.spec.whatwg.org/
[`"exports"`]: packages.md#exports
[`"type"`]: packages.md#type
[`--input-type`]: cli.md#--input-typetype
[`ArrayBuffer`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer
[`SharedArrayBuffer`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer
[`TypedArray`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray
[`Uint8Array`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array
[`data:` URLs]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/Data_URIs
[`export`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/export
[`import()`]: #import-expressions
[`import.meta.resolve`]: #importmetaresolvespecifier-parent
[`import.meta.url`]: #importmetaurl
[`import`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import
[`module.createRequire()`]: module.md#modulecreaterequirefilename
[`module.syncBuiltinESMExports()`]: module.md#modulesyncbuiltinesmexports
[`package.json`]: packages.md#nodejs-packagejson-field-definitions
[`port.ref()`]: https://nodejs.org/dist/latest-v17.x/docs/api/worker_threads.html#portref
[`port.unref()`]: https://nodejs.org/dist/latest-v17.x/docs/api/worker_threads.html#portunref
[`process.dlopen`]: process.md#processdlopenmodule-filename-flags
[`string`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String
[`util.TextDecoder`]: util.md#class-utiltextdecoder
[cjs-module-lexer]: https://github.com/nodejs/cjs-module-lexer/tree/1.2.2
[custom https loader]: #https-loader
[load hook]: #loadurl-context-defaultload
[percent-encoded]: url.md#percent-encoding-in-urls
[resolve hook]: #resolvespecifier-context-defaultresolve
[special scheme]: https://url.spec.whatwg.org/#special-scheme
[status code]: process.md#exit-codes
[the official standard format]: https://tc39.github.io/ecma262/#sec-modules
[url.pathToFileURL]: url.md#urlpathtofileurlpath
