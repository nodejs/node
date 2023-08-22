# Modules: `node:module` API

<!--introduced_in=v12.20.0-->

<!-- YAML
added: v0.3.7
-->

## The `Module` object

* {Object}

Provides general utility methods when interacting with instances of
`Module`, the [`module`][] variable often seen in [CommonJS][] modules. Accessed
via `import 'node:module'` or `require('node:module')`.

### `module.builtinModules`

<!-- YAML
added:
  - v9.3.0
  - v8.10.0
  - v6.13.0
-->

* {string\[]}

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

### `module.isBuiltin(moduleName)`

<!-- YAML
added: v18.6.0
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
added: REPLACEME
-->

> Stability: 1.1 - Active development

* `specifier` {string} Customization hooks to be registered; this should be the
  same string that would be passed to `import()`, except that if it is relative,
  it is resolved relative to `parentURL`.
* `parentURL` {string} If you want to resolve `specifier` relative to a base
  URL, such as `import.meta.url`, you can pass that URL here. **Default:**
  `'data:'`
* `options` {Object}
  * `data` {any} Any arbitrary, cloneable JavaScript value to pass into the
    [`initialize`][] hook.
  * `transferList` {Object\[]} [transferrable objects][] to be passed into the
    `initialize` hook.
* Returns: {any} returns whatever was returned by the `initialize` hook.

Register a module that exports [hooks][] that customize Node.js module
resolution and loading behavior.

```mjs
import { register } from 'node:module';

register('http-to-https', import.meta.url);

// Because this is a dynamic `import()`, the `http-to-https` hooks will run
// before importing `./my-app.mjs`.
await import('./my-app.mjs');
```

In the example above, we are registering the `http-to-https` loader,
but it will only be available for subsequently imported modules—in
this case, `my-app.mjs`. If the `await import('./my-app.mjs')` had
instead been a static `import './my-app.mjs'`, _the app would already
have been loaded_ before the `http-to-https` hooks were
registered. This is part of the design of ES modules, where static
imports are evaluated from the leaves of the tree first back to the
trunk. There can be static imports _within_ `my-app.mjs`, which
will not be evaluated until `my-app.mjs` is when it's dynamically
imported.

The `--experimental-loader` flag of the CLI can be used together
with the `register` function; the hooks registered with the
function will follow the same evaluation chain of hooks registered
within the CLI:

```console
node \
  --experimental-loader unpkg \
  --experimental-loader http-to-https \
  --experimental-loader cache-buster \
  entrypoint.mjs
```

```mjs
// entrypoint.mjs
import { URL } from 'node:url';
import { register } from 'node:module';

const loaderURL = new URL('./my-programmatically-loader.mjs', import.meta.url);

register(loaderURL);
await import('./my-app.mjs');
```

The `my-programmatic-loader.mjs` can leverage `unpkg`,
`http-to-https`, and `cache-buster` loaders.

It's also possible to use `register` more than once:

```mjs
// entrypoint.mjs
import { URL } from 'node:url';
import { register } from 'node:module';

register(new URL('./first-loader.mjs', import.meta.url));
register('./second-loader.mjs', import.meta.url);
await import('./my-app.mjs');
```

Both loaders (`first-loader.mjs` and `second-loader.mjs`) can use
all the resources provided by the loaders registered in the CLI. But
remember that they will only be available in the next imported
module (`my-app.mjs`). The evaluation order of the hooks when
importing `my-app.mjs` and consecutive modules in the example above
will be:

```console
resolve: second-loader.mjs
resolve: first-loader.mjs
resolve: cache-buster
resolve: http-to-https
resolve: unpkg
load: second-loader.mjs
load: first-loader.mjs
load: cache-buster
load: http-to-https
load: unpkg
globalPreload: second-loader.mjs
globalPreload: first-loader.mjs
globalPreload: cache-buster
globalPreload: http-to-https
globalPreload: unpkg
```

This function can also be used to pass data to the loader's [`initialize`][]
hook; the data passed to the hook may include transferrable objects like ports.

```mjs
import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

// This example showcases how a message channel can be used to
// communicate to the loader, by sending `port2` to the loader.
const { port1, port2 } = new MessageChannel();

port1.on('message', (msg) => {
  console.log(msg);
});

register('./my-programmatic-loader.mjs', {
  parentURL: import.meta.url,
  data: { number: 1, port: port2 },
  transferList: [port2],
});
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

## Customization Hooks

<!-- YAML
added: v8.8.0
changes:
  - version: REPLACEME
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

> Stability: 1 - Experimental

> This API is currently being redesigned and will still change.

<!-- type=misc -->

To customize the default module resolution, loader hooks can optionally be
provided via a `--experimental-loader ./loader-name.mjs` argument to Node.js.

When hooks are used they apply to each subsequent loader, the entry point, and
all `import` calls. They won't apply to `require` calls; those still follow
[CommonJS][] rules.

Loaders follow the pattern of `--require`:

```bash
node \
  --experimental-loader unpkg \
  --experimental-loader http-to-https \
  --experimental-loader cache-buster
```

These are called in the following sequence: `cache-buster` calls
`http-to-https` which calls `unpkg`.

### Hooks

Hooks are part of a chain, even if that chain consists of only one custom
(user-provided) hook and the default hook, which is always present. Hook
functions nest: each one must always return a plain object, and chaining happens
as a result of each function calling `next<hookName>()`, which is a reference
to the subsequent loader's hook.

A hook that returns a value lacking a required property triggers an exception.
A hook that returns without calling `next<hookName>()` _and_ without returning
`shortCircuit: true` also triggers an exception. These errors are to help
prevent unintentional breaks in the chain.

Hooks are run in a separate thread, isolated from the main. That means it is a
different [realm](https://tc39.es/ecma262/#realm). The hooks thread may be
terminated by the main thread at any time, so do not depend on asynchronous
operations (like `console.log`) to complete.

#### `initialize()`

<!-- YAML
added: REPLACEME
-->

> The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

* `data` {any} The data from `register(loader, import.meta.url, { data })`.
* Returns: {any} The data to be returned to the caller of `register`.

The `initialize` hook provides a way to define a custom function that runs
in the loader's thread when the loader is initialized. Initialization happens
when the loader is registered via [`register`][] or registered via the
`--experimental-loader` command line option.

This hook can send and receive data from a [`register`][] invocation, including
ports and other transferrable objects. The return value of `initialize` must be
either:

* `undefined`,
* something that can be posted as a message between threads (e.g. the input to
  [`port.postMessage`][]),
* a `Promise` resolving to one of the aforementioned values.

Loader code:

```mjs
// In the below example this file is referenced as
// '/path-to-my-loader.js'

export async function initialize({ number, port }) {
  port.postMessage(`increment: ${number + 1}`);
  return 'ok';
}
```

Caller code:

```mjs
import assert from 'node:assert';
import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

// This example showcases how a message channel can be used to
// communicate between the main (application) thread and the loader
// running on the loaders thread, by sending `port2` to the loader.
const { port1, port2 } = new MessageChannel();

port1.on('message', (msg) => {
  assert.strictEqual(msg, 'increment: 2');
});

const result = register('/path-to-my-loader.js', {
  parentURL: import.meta.url,
  data: { number: 1, port: port2 },
  transferList: [port2],
});

assert.strictEqual(result, 'ok');
```

#### `resolve(specifier, context, nextResolve)`

<!-- YAML
changes:
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

> The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

* `specifier` {string}
* `context` {Object}
  * `conditions` {string\[]} Export conditions of the relevant `package.json`
  * `importAssertions` {Object} An object whose key-value pairs represent the
    assertions for the module to import
  * `parentURL` {string|undefined} The module importing this one, or undefined
    if this is the Node.js entry point
* `nextResolve` {Function} The subsequent `resolve` hook in the chain, or the
  Node.js default `resolve` hook after the last user-supplied `resolve` hook
  * `specifier` {string}
  * `context` {Object}
* Returns: {Object|Promise}
  * `format` {string|null|undefined} A hint to the load hook (it might be
    ignored)
    `'builtin' | 'commonjs' | 'json' | 'module' | 'wasm'`
  * `importAssertions` {Object|undefined} The import assertions to use when
    caching the module (optional; if excluded the input will be used)
  * `shortCircuit` {undefined|boolean} A signal that this hook intends to
    terminate the chain of `resolve` hooks. **Default:** `false`
  * `url` {string} The absolute URL to which this input resolves

> **Caveat** Despite support for returning promises and async functions, calls
> to `resolve` may block the main thread which can impact performance.

The `resolve` hook chain is responsible for telling Node.js where to find and
how to cache a given `import` statement or expression. It can optionally return
its format (such as `'module'`) as a hint to the `load` hook. If a format is
specified, the `load` hook is ultimately responsible for providing the final
`format` value (and it is free to ignore the hint provided by `resolve`); if
`resolve` provides a `format`, a custom `load` hook is required even if only to
pass the value to the Node.js default `load` hook.

Import type assertions are part of the cache key for saving loaded modules into
the internal module cache. The `resolve` hook is responsible for
returning an `importAssertions` object if the module should be cached with
different assertions than were present in the source code.

The `conditions` property in `context` is an array of conditions for
[package exports conditions][Conditional exports] that apply to this resolution
request. They can be used for looking up conditional mappings elsewhere or to
modify the list when calling the default resolution logic.

The current [package exports conditions][Conditional exports] are always in
the `context.conditions` array passed into the hook. To guarantee _default
Node.js module specifier resolution behavior_ when calling `defaultResolve`, the
`context.conditions` array passed to it _must_ include _all_ elements of the
`context.conditions` array originally passed into the `resolve` hook.

```mjs
export function resolve(specifier, context, nextResolve) {
  const { parentURL = null } = context;

  if (Math.random() > 0.5) { // Some condition.
    // For some or all specifiers, do some custom logic for resolving.
    // Always return an object of the form {url: <string>}.
    return {
      shortCircuit: true,
      url: parentURL ?
        new URL(specifier, parentURL).href :
        new URL(specifier).href,
    };
  }

  if (Math.random() < 0.5) { // Another condition.
    // When calling `defaultResolve`, the arguments can be modified. In this
    // case it's adding another value for matching conditional exports.
    return nextResolve(specifier, {
      ...context,
      conditions: [...context.conditions, 'another-condition'],
    });
  }

  // Defer to the next hook in the chain, which would be the
  // Node.js default resolve if this is the last user-specified loader.
  return nextResolve(specifier);
}
```

#### `load(url, context, nextLoad)`

<!-- YAML
changes:
  - version:
    - v18.6.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42623
    description: Add support for chaining load hooks. Each hook must either
      call `nextLoad()` or include a `shortCircuit` property set to `true` in
      its return.
-->

> The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

> In a previous version of this API, this was split across 3 separate, now
> deprecated, hooks (`getFormat`, `getSource`, and `transformSource`).

* `url` {string} The URL returned by the `resolve` chain
* `context` {Object}
  * `conditions` {string\[]} Export conditions of the relevant `package.json`
  * `format` {string|null|undefined} The format optionally supplied by the
    `resolve` hook chain
  * `importAssertions` {Object}
* `nextLoad` {Function} The subsequent `load` hook in the chain, or the
  Node.js default `load` hook after the last user-supplied `load` hook
  * `specifier` {string}
  * `context` {Object}
* Returns: {Object}
  * `format` {string}
  * `shortCircuit` {undefined|boolean} A signal that this hook intends to
    terminate the chain of `resolve` hooks. **Default:** `false`
  * `source` {string|ArrayBuffer|TypedArray} The source for Node.js to evaluate

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
[CommonJS module return value](esm.md#commonjs-namespaces). This limitation
might be overcome in the future.

> **Caveat**: The ESM `load` hook and namespaced exports from CommonJS modules
> are incompatible. Attempting to use them together will result in an empty
> object from the import. This may be addressed in the future.

> These types all correspond to classes defined in ECMAScript.

* The specific [`ArrayBuffer`][] object is a [`SharedArrayBuffer`][].
* The specific [`TypedArray`][] object is a [`Uint8Array`][].

If the source value of a text-based format (i.e., `'json'`, `'module'`)
is not a string, it is converted to a string using [`util.TextDecoder`][].

The `load` hook provides a way to define a custom method for retrieving the
source code of an ES module specifier. This would allow a loader to potentially
avoid reading files from disk. It could also be used to map an unrecognized
format to a supported one, for example `yaml` to `module`.

```mjs
export async function load(url, context, nextLoad) {
  const { format } = context;

  if (Math.random() > 0.5) { // Some condition
    /*
      For some or all URLs, do some custom logic for retrieving the source.
      Always return an object of the form {
        format: <string>,
        source: <string|buffer>,
      }.
    */
    return {
      format,
      shortCircuit: true,
      source: '...',
    };
  }

  // Defer to the next hook in the chain.
  return nextLoad(url);
}
```

In a more advanced scenario, this can also be used to transform an unsupported
source to a supported one (see [Examples](#examples) below).

#### `globalPreload()`

<!-- YAML
changes:
  - version:
    - v18.6.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42623
    description: Add support for chaining globalPreload hooks.
-->

> This hook will be removed in a future version. Use [`initialize`][] instead.
> When a loader has an `initialize` export, `globalPreload` will be ignored.

> In a previous version of this API, this hook was named
> `getGlobalPreloadCode`.

* `context` {Object} Information to assist the preload code
  * `port` {MessagePort}
* Returns: {string} Code to run before application startup

Sometimes it might be necessary to run some code inside of the same global
scope that the application runs in. This hook allows the return of a string
that is run as a sloppy-mode script on startup.

Similar to how CommonJS wrappers work, the code runs in an implicit function
scope. The only argument is a `require`-like function that can be used to load
builtins like "fs": `getBuiltin(request: string)`.

If the code needs more advanced `require` features, it has to construct
its own `require` using  `module.createRequire()`.

```mjs
export function globalPreload(context) {
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

```mjs
/**
 * This example has the application context send a message to the loader
 * and sends the message back to the application context
 */
export function globalPreload({ port }) {
  port.on('message', (msg) => {
    port.postMessage(msg);
  });
  return `\
    port.postMessage('console.log("I went to the Loader and back");');
    port.on('message', (msg) => {
      eval(msg);
    });
  `;
}
```

### Examples

The various loader hooks can be used together to accomplish wide-ranging
customizations of the Node.js code loading and evaluation behaviors.

#### HTTPS loader

In current Node.js, specifiers starting with `https://` are experimental (see
[HTTPS and HTTP imports][]).

The loader below registers hooks to enable rudimentary support for such
specifiers. While this may seem like a significant improvement to Node.js core
functionality, there are substantial downsides to actually using this loader:
performance is much slower than loading files from disk, there is no caching,
and there is no security.

```mjs
// https-loader.mjs
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

With the preceding loader, running
`node --experimental-loader ./https-loader.mjs ./main.mjs`
prints the current version of CoffeeScript per the module at the URL in
`main.mjs`.

#### Transpiler loader

Sources that are in formats Node.js doesn't understand can be converted into
JavaScript using the [`load` hook][load hook].

This is less performant than transpiling source files before running
Node.js; a transpiler loader should only be used for development and testing
purposes.

```mjs
// coffeescript-loader.mjs
import { readFile } from 'node:fs/promises';
import { dirname, extname, resolve as resolvePath } from 'node:path';
import { cwd } from 'node:process';
import { fileURLToPath, pathToFileURL } from 'node:url';
import CoffeeScript from 'coffeescript';

const baseURL = pathToFileURL(`${cwd()}/`).href;

export async function load(url, context, nextLoad) {
  if (extensionsRegex.test(url)) {
    // Now that we patched resolve to let CoffeeScript URLs through, we need to
    // tell Node.js what format such URLs should be interpreted as. Because
    // CoffeeScript transpiles into JavaScript, it should be one of the two
    // JavaScript formats: 'commonjs' or 'module'.

    // CoffeeScript files can be either CommonJS or ES modules, so we want any
    // CoffeeScript file to be treated by Node.js the same as a .js file at the
    // same location. To determine how Node.js would interpret an arbitrary .js
    // file, search up the file system for the nearest parent package.json file
    // and read its "type" field.
    const format = await getPackageType(url);
    // When a hook returns a format of 'commonjs', `source` is ignored.
    // To handle CommonJS files, a handler needs to be registered with
    // `require.extensions` in order to process the files with the CommonJS
    // loader. Avoiding the need for a separate CommonJS handler is a future
    // enhancement planned for ES module loaders.
    if (format === 'commonjs') {
      return {
        format,
        shortCircuit: true,
      };
    }

    const { source: rawSource } = await nextLoad(url, { ...context, format });
    // This hook converts CoffeeScript source code into JavaScript source code
    // for all imported CoffeeScript files.
    const transformedSource = coffeeCompile(rawSource.toString(), url);

    return {
      format,
      shortCircuit: true,
      source: transformedSource,
    };
  }

  // Let Node.js handle all other URLs.
  return nextLoad(url);
}

async function getPackageType(url) {
  // `url` is only a file path during the first iteration when passed the
  // resolved url from the load() hook
  // an actual file path from load() will contain a file extension as it's
  // required by the spec
  // this simple truthy check for whether `url` contains a file extension will
  // work for most projects but does not cover some edge-cases (such as
  // extensionless files or a url ending in a trailing space)
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

import { version } from 'node:process'
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

#### "import map" loader

The previous two loaders defined `load` hooks. This is an example of a loader
that does its work via the `resolve` hook. This loader reads an
`import-map.json` file that specifies which specifiers to override to another
URL (this is a very simplistic implemenation of a small subset of the
"import maps" specification).

```mjs
// import-map-loader.js
import fs from 'node:fs/promises';

const { imports } = JSON.parse(await fs.readFile('import-map.json'));

export async function resolve(specifier, context, nextResolve) {
  if (Object.hasOwn(imports, specifier)) {
    return nextResolve(imports[specifier], context);
  }

  return nextResolve(specifier, context);
}
```

Let's assume we have these files:

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

If you run `node --experimental-loader ./import-map-loader.js main.js`
the output will be `some module!`.

### Register loaders programmatically

<!-- YAML
added: REPLACEME
-->

In addition to using the `--experimental-loader` option in the CLI,
loaders can also be registered programmatically. You can find
detailed information about this process in the documentation page
for [`module.register()`][].

## Source map v3 support

<!-- YAML
added:
 - v13.7.0
 - v12.17.0
-->

> Stability: 1 - Experimental

Helpers for interacting with the source map cache. This cache is
populated when source map parsing is enabled and
[source map include directives][] are found in a modules' footer.

To enable source map parsing, Node.js must be run with the flag
[`--enable-source-maps`][], or with code coverage enabled by setting
[`NODE_V8_COVERAGE=dir`][].

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

### Class: `module.SourceMap`

<!-- YAML
added:
 - v13.7.0
 - v12.17.0
-->

#### `new SourceMap(payload)`

* `payload` {Object}

Creates a new `sourceMap` instance.

`payload` is an object with keys matching the [Source map v3 format][]:

* `file`: {string}
* `version`: {number}
* `sources`: {string\[]}
* `sourcesContent`: {string\[]}
* `names`: {string\[]}
* `mappings`: {string}
* `sourceRoot`: {string}

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

* generatedLine: {number} The line offset of the start of the
  range in the generated source
* generatedColumn: {number} The column offset of start of the
  range in the generated source
* originalSource: {string} The file name of the original source,
  as reported in the SourceMap
* originalLine: {number} The line offset of the start of the
  range in the original source
* originalColumn: {number} The column offset of start of the
  range in the original source
* name: {string}

The returned value represents the raw range as it appears in the
SourceMap, based on zero-indexed offsets, _not_ 1-indexed line and
column numbers as they appear in Error messages and CallSite
objects.

To get the corresponding 1-indexed line and column numbers from a
lineNumber and columnNumber as they are reported by Error stacks
and CallSite objects, use `sourceMap.findOrigin(lineNumber,
columnNumber)`

#### `sourceMap.findOrigin(lineNumber, columnNumber)`

* `lineNumber` {number} The 1-indexed line number of the call
  site in the generated source
* `columnOffset` {number} The 1-indexed column number
  of the call site in the generated source
* Returns: {Object}

Given a 1-indexed lineNumber and columnNumber from a call site in
the generated source, find the corresponding call site location
in the original source.

If the lineNumber and columnNumber provided are not found in any
source map, then an empty object is returned.  Otherwise, the
returned object contains the following keys:

* name: {string | undefined} The name of the range in the
  source map, if one was provided
* fileName: {string} The file name of the original source, as
  reported in the SourceMap
* lineNumber: {number} The 1-indexed lineNumber of the
  corresponding call site in the original source
* columnNumber: {number} The 1-indexed columnNumber of the
  corresponding call site in the original source

[CommonJS]: modules.md
[Conditional exports]: packages.md#conditional-exports
[ES Modules]: esm.md
[HTTPS and HTTP imports]: esm.md#https-and-http-imports
[Source map v3 format]: https://sourcemaps.info/spec.html#h.mofvlxcwqzej
[`--enable-source-maps`]: cli.md#--enable-source-maps
[`ArrayBuffer`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer
[`NODE_V8_COVERAGE=dir`]: cli.md#node_v8_coveragedir
[`SharedArrayBuffer`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer
[`SourceMap`]: #class-modulesourcemap
[`TypedArray`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray
[`Uint8Array`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array
[`initialize`]: #initialize
[`module.register()`]: #moduleregisterspecifier-parenturl-options
[`module`]: modules.md#the-module-object
[`port.postMessage`]: worker_threads.md#portpostmessagevalue-transferlist
[`port.ref()`]: worker_threads.md#portref
[`port.unref()`]: worker_threads.md#portunref
[`register`]: #moduleregisterspecifier-parenturl-options
[`string`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String
[`util.TextDecoder`]: util.md#class-utiltextdecoder
[hooks]: #customization-hooks
[load hook]: #loadurl-context-nextload
[module wrapper]: modules.md#the-module-wrapper
[source map include directives]: https://sourcemaps.info/spec.html#h.lmz475t4mvbx
[transferrable objects]: worker_threads.md#portpostmessagevalue-transferlist
