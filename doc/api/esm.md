# ECMAScript Modules

<!--introduced_in=v8.5.0-->
<!-- type=misc -->

> Stability: 1 - Experimental

<!--name=esm-->

Node.js contains support for ES Modules based upon the
[Node.js EP for ES Modules][] and the [ESM Minimal Kernel][].

The minimal feature set is designed to be compatible with all potential
future implementations. Expect major changes in the implementation including
interoperability support, specifier resolution, and default behavior.

## Enabling

<!-- type=misc -->

The `--experimental-modules` flag can be used to enable features for loading
ESM modules.

Once this has been set, files ending with `.mjs` will be able to be loaded
as ES Modules.

```sh
node --experimental-modules my-app.mjs
```

## Features

<!-- type=misc -->

### Supported

Only the CLI argument for the main entry point to the program can be an entry
point into an ESM graph. Dynamic import can also be used to create entry points
into ESM graphs at runtime.

#### import.meta

* {Object}

The `import.meta` metaproperty is an `Object` that contains the following
property:

* `url` {string} The absolute `file:` URL of the module.

### Unsupported

| Feature | Reason |
| --- | --- |
| `require('./foo.mjs')` | ES Modules have differing resolution and timing, use dynamic import |

## Notable differences between `import` and `require`

### Mandatory file extensions

You must provide a file extension when using the `import` keyword.

### No NODE_PATH

`NODE_PATH` is not part of resolving `import` specifiers. Please use symlinks
if this behavior is desired.

### No `require.extensions`

`require.extensions` is not used by `import`. The expectation is that loader
hooks can provide this workflow in the future.

### No `require.cache`

`require.cache` is not used by `import`. It has a separate cache.

### URL based paths

ESM are resolved and cached based upon [URL](https://url.spec.whatwg.org/)
semantics. This means that files containing special characters such as `#` and
`?` need to be escaped.

Modules will be loaded multiple times if the `import` specifier used to resolve
them have a different query or fragment.

```js
import './foo.mjs?query=1'; // loads ./foo.mjs with query of "?query=1"
import './foo.mjs?query=2'; // loads ./foo.mjs with query of "?query=2"
```

For now, only modules using the `file:` protocol can be loaded.

## CommonJS, JSON, and Native Modules

CommonJS, JSON, and Native modules can be used with [`module.createRequireFromPath()`][].

```js
// cjs.js
module.exports = 'cjs';

// esm.mjs
import { createRequireFromPath as createRequire } from 'module';
import { fileURLToPath as fromPath } from 'url';

const require = createRequire(fromPath(import.meta.url));

const cjs = require('./cjs');
cjs === 'cjs'; // true
```

## Builtin modules

Builtin modules will provide named exports of their public API, as well as a
default export which can be used for, among other things, modifying the named
exports. Named exports of builtin modules are updated when the corresponding
exports property is accessed, redefined, or deleted.

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

fs.readFileSync = () => Buffer.from('Hello, ESM');

fs.readFileSync === readFileSync;
```

## Resolution Algorithm

### Features

The resolver has the following properties:

* FileURL-based resolution as is used by ES modules
* Support for builtin module loading
* Relative and absolute URL resolution
* No default extensions
* No folder mains
* Bare specifier package resolution lookup through node_modules

### Resolver Algorithm

The algorithm to load an ES module specifier is given through the
**ESM_RESOLVE** method below. It returns the resolved URL for a 
module specifier relative to a parentURL, in addition to the unique module
format for that resolved URL given by the **ESM_FORMAT** routine.

In the following algorithms, all subroutine errors are propogated as errors
of these top-level routines.

#### ESM_RESOLVE(_specifier_, _parentURL_)
> 1. Let _resolvedURL_ be *undefined*.
> 1. If _specifier_ is a valid URL, then
>    1. Set _resolvedURL_ to the result of parsing and reserializing
>       _specifier_ as a URL.
> 1. Otherwise, if _specifier_ starts with _"/"_, then
>    1. Throw an _Invalid Specifier_ error.
> 1. Otherwise, if _specifier_ starts with _"./"_ or _"../"_, then
>    1. Set _resolvedURL_ to the URL resolution of _specifier_ relative to
>       _parentURL_.
> 1. Otherwise,
>    1. Note: _specifier_ is now a bare specifier.
>    1. Set _resolvedURL_ the result of
>       **PACKAGE_RESOLVE**(_specifier_, _parentURL_).
> 1. If the file at _resolvedURL_ does not exist, then
>    1. Throw a _Module Not Found_ error.
> 1. Let _format_ be the result of **ESM_FORMAT**(_url_).
> 1. Return _{ resolvedURL, format }_.

PACKAGE_RESOLVE(_packageSpecifier_, _parentURL_)
> 1. Let _packageName_ be *undefined*.
> 1. Let _packagePath_ be *undefined*.
> 1. If _packageSpecifier_ does not start with _"@"_, then
>    1. If _packageSpecifier_ is an empty string, then
>       1. Throw an _Invalid Package Name_ error.
>    1. Set _packageName_ to the substring of _packageSpecifier_ until the
>       first _"/"_ separator or the end of the string.
> 1. If _packageSpecifier_ starts with _"@"_, then
>    1. If _packageSpecifier_ does not contain a _"/"_ separator, then
>       1. Throw an _Invalid Package Name_ error.
>    1. Set _packageName_ to the substring of _packageSpecifier_
>       until the second _"/"_ separator or the end of the string.
> 1. Let _packagePath_ be the substring of _packageSpecifier_ from the
>    position at the length of _packageName_ plus one, if any.
> 1. Assert: _packageName_ is a valid package name or scoped package name.
> 1. Assert: _packagePath_ is either empty, or a path without a leading
>    separator.
> 1. Note: Further package name validations can be added here.
> 1. If _packagePath_ is empty and _packageName_ is a Node.js builtin
>    module, then
>    1. Return the string _"node:"_ concatenated with _packageSpecifier_.
> 1. While _parentURL_ is not the file system root,
>    1. Set _parentURL_ to the parent folder URL of _parentURL_.
>    1. Let _packageURL_ be the URL resolution of the string concatenation of
>       _parentURL_, _"/node_modules/"_ and _packageSpecifier_.
>    1. If the folder at _packageURL_ does not exist, then
>       1. Note: This check can be optimized out where possible in
>          implementation.
>       1. Set _parentURL_ to the parent URL path of _parentURL_.
>       1. Continue the next loop iteration.
>    1. If _packagePath_ is empty, then
>       1. Let _url_ be the result of **PACKAGE_MAIN_RESOLVE**(_packageURL_).
>       1. If _url_ is *null*, then
>          1. Throw a _Module Not Found_ error.
>       1. Return _url_.
>    1. Otherwise,
>       1. Return the URL resolution of _packagePath_ in _packageURL_.
> 1. Throw a _Module Not Found_ error.

PACKAGE_MAIN_RESOLVE(_packageURL_)
> 1. Let _pjsonURL_ be the URL of the file _"package.json"_ within the parent
     path _packageURL_.
> 1. If the file at _pjsonURL_ exists, then
>    1. Let _pjson_ be the result of **READ_JSON_FILE**(_pjsonURL_).
>    1. If **HAS_ESM_PROPERTIES**(_pjson_) is *true*, then
>       1. Let _mainURL_ be the result applying the legacy
>          **LOAD_AS_DIRECTORY** CommonJS resolver to _packageURL_, returning
>          *undefined* for no resolution.
>       1. If _mainURL_ is not *undefined* and **ESM_FORMAT**(_mainURL_) is not
>          equal to _"cjs"_, then
>          1. Throw a _"Invalid Module Format"_ error.
>       1. Return _mainURL_.
> 1. _Note: ESM main yet to be implemented here._
> 1. Return *null*.

#### ESM_FORMAT(_url_)
> 1. Assert: _url_ corresponds to an existing file.
> 1. Let _pjsonURL_ be the result of **READ_PACKAGE_BOUNDARY**(_url_).
> 1. Let _pjson_ be *undefined*.
> 1. If _pjsonURL_ is not *null*, then
>    1. Set _pjson_ to the result of **READ_JSON_FILE**(_pjsonURL_).
> 1. If _pjsonURL_ is *null* or **HAS_ESM_PROPERTIES**(_pjson_) is *true*, then
>    1. If _url_ does not end in _".js"_ or _".mjs"_ then,
>       1. Throw an _Unkonwn Module Format_ error.
>    1. Return _"esm"_.
> 1. Otherwise,
>    1. If _url_ does not end in _".js"_ then,
>       1. Throw an _Unknown Module Format_ error.
>    1. Return _"cjs"_.

> 1. If **HAS_ESM_PROPERTIES**(_pjson_) is *true*, then
>    1. Return _"esm"_.
> 1. Return _"cjs"_.

READ_PACKAGE_BOUNDARY(_url_)
> 1. Let _boundaryURL_ be the URL resolution of _"package.json"_ relative to
>    _url_.
> 1. While _boundaryURL_ is not the file system root,
>    1. If the file at _boundaryURL_ exists, then
>       1. Return _boundaryURL_.
>    1. Set _boundaryURL_ to the URL resolution of _"../package.json"_ relative
>       to _boundaryURL_.
> 1. Return *null*.

READ_JSON_FILE(_url_)
> 1. If the file at _url_ does not parse as valid JSON, then
>    1. Throw an _Invalid Package Configuration_ error.
> 1. Let _pjson_ be the parsed JSON source of the file at _url_.
> 1. Return _pjson_.

HAS_ESM_PROPERTIES(_pjson_)
> 1. Note: To be specified.

_ESM properties_ in a package.json file are yet to be specified.
The current possible specifications for this are the
[_"exports"_](https://github.com/jkrems/proposal-pkg-exports)
or [_"mode"_](https://github.com/nodejs/node/pull/18392) flag.

[Node.js EP for ES Modules]: https://github.com/nodejs/node-eps/blob/master/002-es-modules.md
[`module.createRequireFromPath()`]: modules.html#modules_module_createrequirefrompath_filename
[ESM Minimal Kernel]: https://github.com/nodejs/modules/blob/master/doc/plan-for-new-modules-implementation.md
