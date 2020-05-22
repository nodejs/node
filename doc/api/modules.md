# Modules

<!--introduced_in=v0.10.0-->

## Introduction

### CommonJS modules

> Stability: 2 - Stable

<!--name=module-->

In the Node.js module system, each file is treated as a separate module. For
example, consider a file named `foo.js`:

```js
const circle = require('./circle.js');
console.log(`The area of a circle of radius 4 is ${circle.area(4)}`);
```

On the first line, `foo.js` loads the module `circle.js` that is in the same
directory as `foo.js`.

Here are the contents of `circle.js`:

```js
const { PI } = Math;

exports.area = (r) => PI * r ** 2;

exports.circumference = (r) => 2 * PI * r;
```

The module `circle.js` has exported the functions `area()` and
`circumference()`. Functions and objects are added to the root of a module
by specifying additional properties on the special `exports` object.

Variables local to the module will be private, because the module is wrapped
in a function by Node.js (see [module wrapper](#modules_the_module_wrapper)).
In this example, the variable `PI` is private to `circle.js`.

The `module.exports` property can be assigned a new value (such as a function
or object).

Below, `bar.js` makes use of the `square` module, which exports a Square class:

```js
const Square = require('./square.js');
const mySquare = new Square(2);
console.log(`The area of mySquare is ${mySquare.area()}`);
```

The `square` module is defined in `square.js`:

```js
// Assigning to exports will not modify module, must use module.exports
module.exports = class Square {
  constructor(width) {
    this.width = width;
  }

  area() {
    return this.width ** 2;
  }
};
```

The module system is implemented in the `require('module')` module.

### ECMAScript modules

<!--introduced_in=v8.5.0-->
<!--name=esm-->

> Stability: 1 - Experimental

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
provides limited interoperability between them and the existing module format,
[CommonJS][].

Node.js contains support for ES Modules based upon the
[Node.js EP for ES Modules][] and the [ECMAScript-modules implementation][].

Expect major changes in the implementation including interoperability support,
specifier resolution, and default behavior.

## Usage

Node.js will treat the following as ES modules when passed to `node` as the
initial input, or when referenced by `import` statements within ES module code:

* Files ending in `.mjs`.

* Files ending in `.js` when the nearest parent `package.json` file contains a
  top-level field `"type"` with a value of `"module"`.

* Strings passed in as an argument to `--eval`, or piped to `node` via `STDIN`,
  with the flag `--input-type=module`.

Node.js will treat as CommonJS all other forms of input, such as `.js` files
where the nearest parent `package.json` file contains no top-level `"type"`
field, or string input without the flag `--input-type`. This behavior is to
preserve backward compatibility. However, now that Node.js supports both
CommonJS and ES modules, it is best to be explicit whenever possible. Node.js
will treat the following as CommonJS when passed to `node` as the initial input,
or when referenced by `import` statements within ES module code:

* Files ending in `.cjs`.

* Files ending in `.js` when the nearest parent `package.json` file contains a
  top-level field `"type"` with a value of `"commonjs"`.

* Strings passed in as an argument to `--eval` or `--print`, or piped to `node`
  via `STDIN`, with the flag `--input-type=commonjs`.

### `package.json` `"type"` field

Files ending with `.js` will be loaded as ES modules when the nearest parent
`package.json` file contains a top-level field `"type"` with a value of
`"module"`.

The nearest parent `package.json` is defined as the first `package.json` found
when searching in the current folder, that folder’s parent, and so on up
until the root of the volume is reached.

```json
// package.json
{
  "type": "module"
}
```

```sh
# In same folder as above package.json
node my-app.js # Runs as ES module
```

If the nearest parent `package.json` lacks a `"type"` field, or contains
`"type": "commonjs"`, `.js` files are treated as CommonJS. If the volume root is
reached and no `package.json` is found, Node.js defers to the default, a
`package.json` with no `"type"` field.

`import` statements of `.js` files are treated as ES modules if the nearest
parent `package.json` contains `"type": "module"`.

```js
// my-app.js, part of the same example as above
import './startup.js'; // Loaded as ES module because of package.json
```

Package authors should include the `"type"` field, even in packages where all
sources are CommonJS. Being explicit about the `type` of the package will
future-proof the package in case the default type of Node.js ever changes, and
it will also make things easier for build tools and loaders to determine how the
files in the package should be interpreted.

Regardless of the value of the `"type"` field, `.mjs` files are always treated
as ES modules and `.cjs` files are always treated as CommonJS.

### Package Scope and File Extensions

A folder containing a `package.json` file, and all subfolders below that folder
down until the next folder containing another `package.json`, is considered a
_package scope_. The `"type"` field defines how `.js` files should be treated
within a particular `package.json` file’s package scope. Every package in a
project’s `node_modules` folder contains its own `package.json` file, so each
project’s dependencies have their own package scopes. A `package.json` lacking a
`"type"` field is treated as if it contained `"type": "commonjs"`.

The package scope applies not only to initial entry points (`node my-app.js`)
but also to files referenced by `import` statements and `import()` expressions.

```js
// my-app.js, in an ES module package scope because there is a package.json
// file in the same folder with "type": "module".

import './startup/init.js';
// Loaded as ES module since ./startup contains no package.json file,
// and therefore inherits the ES module package scope from one level up.

import 'commonjs-package';
// Loaded as CommonJS since ./node_modules/commonjs-package/package.json
// lacks a "type" field or contains "type": "commonjs".

import './node_modules/commonjs-package/index.js';
// Loaded as CommonJS since ./node_modules/commonjs-package/package.json
// lacks a "type" field or contains "type": "commonjs".
```

Files ending with `.mjs` are always loaded as ES modules regardless of package
scope.

Files ending with `.cjs` are always loaded as CommonJS regardless of package
scope.

```js
import './legacy-file.cjs';
// Loaded as CommonJS since .cjs is always loaded as CommonJS.

import 'commonjs-package/src/index.mjs';
// Loaded as ES module since .mjs is always loaded as ES module.
```

The `.mjs` and `.cjs` extensions may be used to mix types within the same
package scope:

* Within a `"type": "module"` package scope, Node.js can be instructed to
  interpret a particular file as CommonJS by naming it with a `.cjs` extension
  (since both `.js` and `.mjs` files are treated as ES modules within a
  `"module"` package scope).

* Within a `"type": "commonjs"` package scope, Node.js can be instructed to
  interpret a particular file as an ES module by naming it with an `.mjs`
  extension (since both `.js` and `.cjs` files are treated as CommonJS within a
  `"commonjs"` package scope).

### `--input-type` flag

Strings passed in as an argument to `--eval` (or `-e`), or piped to `node` via
`STDIN`, will be treated as ES modules when the `--input-type=module` flag is
set.

```sh
node --input-type=module --eval "import { sep } from 'path'; console.log(sep);"

echo "import { sep } from 'path'; console.log(sep);" | node --input-type=module
```

For completeness there is also `--input-type=commonjs`, for explicitly running
string input as CommonJS. This is the default behavior if `--input-type` is
unspecified.

## CommonJS modules

### Accessing the main module

<!-- type=misc -->

When a file is run directly from Node.js, `require.main` is set to its
`module`. That means that it is possible to determine whether a file has been
run directly by testing `require.main === module`.

For a file `foo.js`, this will be `true` if run via `node foo.js`, but
`false` if run by `require('./foo')`.

Because `module` provides a `filename` property (normally equivalent to
`__filename`), the entry point of the current application can be obtained
by checking `require.main.filename`.

### Addenda: The `.mjs` extension

It is not possible to `require()` files that have the `.mjs` extension.
Attempting to do so will throw [an error][]. The `.mjs` extension is
reserved for [ECMAScript Modules][] which cannot be loaded via `require()`.
See [ECMAScript Modules][] for more details.

### Caching

<!--type=misc-->

Modules are cached after the first time they are loaded. This means (among other
things) that every call to `require('foo')` will get exactly the same object
returned, if it would resolve to the same file.

Provided `require.cache` is not modified, multiple calls to `require('foo')`
will not cause the module code to be executed multiple times. This is an
important feature. With it, "partially done" objects can be returned, thus
allowing transitive dependencies to be loaded even when they would cause cycles.

To have a module execute code multiple times, export a function, and call that
function.

#### Module Caching Caveats

<!--type=misc-->

Modules are cached based on their resolved filename. Since modules may resolve
to a different filename based on the location of the calling module (loading
from `node_modules` folders), it is not a *guarantee* that `require('foo')` will
always return the exact same object, if it would resolve to different files.

Additionally, on case-insensitive file systems or operating systems, different
resolved filenames can point to the same file, but the cache will still treat
them as different modules and will reload the file multiple times. For example,
`require('./foo')` and `require('./FOO')` return two different objects,
irrespective of whether or not `./foo` and `./FOO` are the same file.

### Core Modules

<!--type=misc-->

Node.js has several modules compiled into the binary. These modules are
described in greater detail elsewhere in this documentation.

The core modules are defined within the Node.js source and are located in the
`lib/` folder.

Core modules are always preferentially loaded if their identifier is
passed to `require()`. For instance, `require('http')` will always
return the built in HTTP module, even if there is a file by that name.

### Cycles

<!--type=misc-->

When there are circular `require()` calls, a module might not have finished
executing when it is returned.

Consider this situation:

`a.js`:

```js
console.log('a starting');
exports.done = false;
const b = require('./b.js');
console.log('in a, b.done = %j', b.done);
exports.done = true;
console.log('a done');
```

`b.js`:

```js
console.log('b starting');
exports.done = false;
const a = require('./a.js');
console.log('in b, a.done = %j', a.done);
exports.done = true;
console.log('b done');
```

`main.js`:

```js
console.log('main starting');
const a = require('./a.js');
const b = require('./b.js');
console.log('in main, a.done = %j, b.done = %j', a.done, b.done);
```

When `main.js` loads `a.js`, then `a.js` in turn loads `b.js`. At that
point, `b.js` tries to load `a.js`. In order to prevent an infinite
loop, an **unfinished copy** of the `a.js` exports object is returned to the
`b.js` module. `b.js` then finishes loading, and its `exports` object is
provided to the `a.js` module.

By the time `main.js` has loaded both modules, they're both finished.
The output of this program would thus be:

```console
$ node main.js
main starting
a starting
b starting
in b, a.done = false
b done
in a, b.done = true
a done
in main, a.done = true, b.done = true
```

Careful planning is required to allow cyclic module dependencies to work
correctly within an application.

### File Modules

<!--type=misc-->

If the exact filename is not found, then Node.js will attempt to load the
required filename with the added extensions: `.js`, `.json`, and finally
`.node`.

`.js` files are interpreted as JavaScript text files, and `.json` files are
parsed as JSON text files. `.node` files are interpreted as compiled addon
modules loaded with `process.dlopen()`.

A required module prefixed with `'/'` is an absolute path to the file. For
example, `require('/home/marco/foo.js')` will load the file at
`/home/marco/foo.js`.

A required module prefixed with `'./'` is relative to the file calling
`require()`. That is, `circle.js` must be in the same directory as `foo.js` for
`require('./circle')` to find it.

Without a leading `'/'`, `'./'`, or `'../'` to indicate a file, the module must
either be a core module or is loaded from a `node_modules` folder.

If the given path does not exist, `require()` will throw an [`Error`][] with its
`code` property set to `'MODULE_NOT_FOUND'`.

### Folders as Modules

<!--type=misc-->

It is convenient to organize programs and libraries into self-contained
directories, and then provide a single entry point to those directories.
There are three ways in which a folder may be passed to `require()` as
an argument.

The first is to create a `package.json` file in the root of the folder,
which specifies a `main` module. An example `package.json` file might
look like this:

```json
{ "name" : "some-library",
  "main" : "./lib/some-library.js" }
```

If this was in a folder at `./some-library`, then
`require('./some-library')` would attempt to load
`./some-library/lib/some-library.js`.

This is the extent of the awareness of `package.json` files within Node.js.

If there is no `package.json` file present in the directory, or if the
`'main'` entry is missing or cannot be resolved, then Node.js
will attempt to load an `index.js` or `index.node` file out of that
directory. For example, if there was no `package.json` file in the above
example, then `require('./some-library')` would attempt to load:

* `./some-library/index.js`
* `./some-library/index.node`

If these attempts fail, then Node.js will report the entire module as missing
with the default error:

```txt
Error: Cannot find module 'some-library'
```

### Loading from `node_modules` Folders

<!--type=misc-->

If the module identifier passed to `require()` is not a
[core](#modules_core_modules) module, and does not begin with `'/'`, `'../'`, or
`'./'`, then Node.js starts at the parent directory of the current module, and
adds `/node_modules`, and attempts to load the module from that location.
Node.js will not append `node_modules` to a path already ending in
`node_modules`.

If it is not found there, then it moves to the parent directory, and so
on, until the root of the file system is reached.

For example, if the file at `'/home/ry/projects/foo.js'` called
`require('bar.js')`, then Node.js would look in the following locations, in
this order:

* `/home/ry/projects/node_modules/bar.js`
* `/home/ry/node_modules/bar.js`
* `/home/node_modules/bar.js`
* `/node_modules/bar.js`

This allows programs to localize their dependencies, so that they do not
clash.

It is possible to require specific files or sub modules distributed with a
module by including a path suffix after the module name. For instance
`require('example-module/path/to/file')` would resolve `path/to/file`
relative to where `example-module` is located. The suffixed path follows the
same module resolution semantics.

### Loading from the global folders

<!-- type=misc -->

If the `NODE_PATH` environment variable is set to a colon-delimited list
of absolute paths, then Node.js will search those paths for modules if they
are not found elsewhere.

On Windows, `NODE_PATH` is delimited by semicolons (`;`) instead of colons.

`NODE_PATH` was originally created to support loading modules from
varying paths before the current [module resolution][] algorithm was defined.

`NODE_PATH` is still supported, but is less necessary now that the Node.js
ecosystem has settled on a convention for locating dependent modules.
Sometimes deployments that rely on `NODE_PATH` show surprising behavior
when people are unaware that `NODE_PATH` must be set. Sometimes a
module's dependencies change, causing a different version (or even a
different module) to be loaded as the `NODE_PATH` is searched.

Additionally, Node.js will search in the following list of GLOBAL_FOLDERS:

* 1: `$HOME/.node_modules`
* 2: `$HOME/.node_libraries`
* 3: `$PREFIX/lib/node`

Where `$HOME` is the user's home directory, and `$PREFIX` is the Node.js
configured `node_prefix`.

These are mostly for historic reasons.

It is strongly encouraged to place dependencies in the local `node_modules`
folder. These will be loaded faster, and more reliably.

### The module wrapper

<!-- type=misc -->

Before a module's code is executed, Node.js will wrap it with a function
wrapper that looks like the following:

```js
(function(exports, require, module, __filename, __dirname) {
// Module code actually lives in here
});
```

By doing this, Node.js achieves a few things:

* It keeps top-level variables (defined with `var`, `const` or `let`) scoped to
the module rather than the global object.
* It helps to provide some global-looking variables that are actually specific
  to the module, such as:
  * The `module` and `exports` objects that the implementor can use to export
    values from the module.
  * The convenience variables `__filename` and `__dirname`, containing the
    module's absolute filename and directory path.

### The module scope

#### `__dirname`
<!-- YAML
added: v0.1.27
-->

<!-- type=var -->

* {string}

The directory name of the current module. This is the same as the
[`path.dirname()`][] of the [`__filename`][].

Example: running `node example.js` from `/Users/mjr`

```js
console.log(__dirname);
// Prints: /Users/mjr
console.log(path.dirname(__filename));
// Prints: /Users/mjr
```

#### `__filename`
<!-- YAML
added: v0.0.1
-->

<!-- type=var -->

* {string}

The file name of the current module. This is the current module file's absolute
path with symlinks resolved.

For a main program this is not necessarily the same as the file name used in the
command line.

See [`__dirname`][] for the directory name of the current module.

Examples:

Running `node example.js` from `/Users/mjr`

```js
console.log(__filename);
// Prints: /Users/mjr/example.js
console.log(__dirname);
// Prints: /Users/mjr
```

Given two modules: `a` and `b`, where `b` is a dependency of
`a` and there is a directory structure of:

* `/Users/mjr/app/a.js`
* `/Users/mjr/app/node_modules/b/b.js`

References to `__filename` within `b.js` will return
`/Users/mjr/app/node_modules/b/b.js` while references to `__filename` within
`a.js` will return `/Users/mjr/app/a.js`.

#### `exports`
<!-- YAML
added: v0.1.12
-->

<!-- type=var -->

* {Object}

A reference to the `module.exports` that is shorter to type.
See the section about the [exports shortcut][] for details on when to use
`exports` and when to use `module.exports`.

#### `module`
<!-- YAML
added: v0.1.16
-->

<!-- type=var -->

* {module}

A reference to the current module, see the section about the
[`module` object][]. In particular, `module.exports` is used for defining what
a module exports and makes available through `require()`.

#### `require(id)`
<!-- YAML
added: v0.1.13
-->

<!-- type=var -->

* `id` {string} module name or path
* Returns: {any} exported module content

Used to import modules, `JSON`, and local files. Modules can be imported
from `node_modules`. Local modules and JSON files can be imported using
a relative path (e.g. `./`, `./foo`, `./bar/baz`, `../foo`) that will be
resolved against the directory named by [`__dirname`][] (if defined) or
the current working directory. The relative paths of POSIX style are resolved
in an OS independent fashion, meaning that the examples above will work on
Windows in the same way they would on Unix systems.

```js
// Importing a local module with a path relative to the `__dirname` or current
// working directory. (On Windows, this would resolve to .\path\myLocalModule.)
const myLocalModule = require('./path/myLocalModule');

// Importing a JSON file:
const jsonData = require('./path/filename.json');

// Importing a module from node_modules or Node.js built-in module:
const crypto = require('crypto');
```

##### `require.cache`
<!-- YAML
added: v0.3.0
-->

* {Object}

Modules are cached in this object when they are required. By deleting a key
value from this object, the next `require` will reload the module.
This does not apply to [native addons][], for which reloading will result in an
error.

Adding or replacing entries is also possible. This cache is checked before
native modules and if a name matching a native module is added to the cache,
no require call is
going to receive the native module anymore. Use with care!

##### `require.extensions`
<!-- YAML
added: v0.3.0
deprecated: v0.10.6
-->

> Stability: 0 - Deprecated

* {Object}

Instruct `require` on how to handle certain file extensions.

Process files with the extension `.sjs` as `.js`:

```js
require.extensions['.sjs'] = require.extensions['.js'];
```

**Deprecated.** In the past, this list has been used to load non-JavaScript
modules into Node.js by compiling them on-demand. However, in practice, there
are much better ways to do this, such as loading modules via some other Node.js
program, or compiling them to JavaScript ahead of time.

Avoid using `require.extensions`. Use could cause subtle bugs and resolving the
extensions gets slower with each registered extension.

##### `require.main`
<!-- YAML
added: v0.1.17
-->

* {module}

The `Module` object representing the entry script loaded when the Node.js
process launched.
See ["Accessing the main module"](#modules_accessing_the_main_module).

In `entry.js` script:

```js
console.log(require.main);
```

```sh
node entry.js
```

<!-- eslint-skip -->
```js
Module {
  id: '.',
  path: '/absolute/path/to',
  exports: {},
  parent: null,
  filename: '/absolute/path/to/entry.js',
  loaded: false,
  children: [],
  paths:
   [ '/absolute/path/to/node_modules',
     '/absolute/path/node_modules',
     '/absolute/node_modules',
     '/node_modules' ] }
```

##### `require.resolve(request[, options])`
<!-- YAML
added: v0.3.0
changes:
  - version: v8.9.0
    pr-url: https://github.com/nodejs/node/pull/16397
    description: The `paths` option is now supported.
-->

* `request` {string} The module path to resolve.
* `options` {Object}
  * `paths` {string[]} Paths to resolve module location from. If present, these
    paths are used instead of the default resolution paths, with the exception
    of [GLOBAL_FOLDERS][] like `$HOME/.node_modules`, which are always
    included. Each of these paths is used as a starting point for
    the module resolution algorithm, meaning that the `node_modules` hierarchy
    is checked from this location.
* Returns: {string}

Use the internal `require()` machinery to look up the location of a module,
but rather than loading the module, just return the resolved filename.

If the module can not be found, a `MODULE_NOT_FOUND` error is thrown.

###### `require.resolve.paths(request)`
<!-- YAML
added: v8.9.0
-->

* `request` {string} The module path whose lookup paths are being retrieved.
* Returns: {string[]|null}

Returns an array containing the paths searched during resolution of `request` or
`null` if the `request` string references a core module, for example `http` or
`fs`.

### The `module` Object
<!-- YAML
added: v0.1.16
-->

<!-- type=var -->
<!-- name=module -->

* {Object}

In each module, the `module` free variable is a reference to the object
representing the current module. For convenience, `module.exports` is
also accessible via the `exports` module-global. `module` is not actually
a global but rather local to each module.

#### `module.children`
<!-- YAML
added: v0.1.16
-->

* {module[]}

The module objects required for the first time by this one.

#### `module.exports`
<!-- YAML
added: v0.1.16
-->

* {Object}

The `module.exports` object is created by the `Module` system. Sometimes this is
not acceptable; many want their module to be an instance of some class. To do
this, assign the desired export object to `module.exports`. Assigning
the desired object to `exports` will simply rebind the local `exports` variable,
which is probably not what is desired.

For example, suppose we were making a module called `a.js`:

```js
const EventEmitter = require('events');

module.exports = new EventEmitter();

// Do some work, and after some time emit
// the 'ready' event from the module itself.
setTimeout(() => {
  module.exports.emit('ready');
}, 1000);
```

Then in another file we could do:

```js
const a = require('./a');
a.on('ready', () => {
  console.log('module "a" is ready');
});
```

Assignment to `module.exports` must be done immediately. It cannot be
done in any callbacks. This does not work:

`x.js`:

```js
setTimeout(() => {
  module.exports = { a: 'hello' };
}, 0);
```

`y.js`:

```js
const x = require('./x');
console.log(x.a);
```

##### `exports` shortcut
<!-- YAML
added: v0.1.16
-->

The `exports` variable is available within a module's file-level scope, and is
assigned the value of `module.exports` before the module is evaluated.

It allows a shortcut, so that `module.exports.f = ...` can be written more
succinctly as `exports.f = ...`. However, be aware that like any variable, if a
new value is assigned to `exports`, it is no longer bound to `module.exports`:

```js
module.exports.hello = true; // Exported from require of module
exports = { hello: false };  // Not exported, only available in the module
```

When the `module.exports` property is being completely replaced by a new
object, it is common to also reassign `exports`:

<!-- eslint-disable func-name-matching -->
```js
module.exports = exports = function Constructor() {
  // ... etc.
};
```

To illustrate the behavior, imagine this hypothetical implementation of
`require()`, which is quite similar to what is actually done by `require()`:

```js
function require(/* ... */) {
  const module = { exports: {} };
  ((module, exports) => {
    // Module code here. In this example, define a function.
    function someFunc() {}
    exports = someFunc;
    // At this point, exports is no longer a shortcut to module.exports, and
    // this module will still export an empty default object.
    module.exports = someFunc;
    // At this point, the module will now export someFunc, instead of the
    // default object.
  })(module, module.exports);
  return module.exports;
}
```

#### `module.filename`
<!-- YAML
added: v0.1.16
-->

* {string}

The fully resolved filename of the module.

#### `module.id`
<!-- YAML
added: v0.1.16
-->

* {string}

The identifier for the module. Typically this is the fully resolved
filename.

#### `module.loaded`
<!-- YAML
added: v0.1.16
-->

* {boolean}

Whether or not the module is done loading, or is in the process of
loading.

#### `module.parent`
<!-- YAML
added: v0.1.16
-->

* {module}

The module that first required this one.

#### `module.path`
<!-- YAML
added: v11.14.0
-->

* {string}

The directory name of the module. This is usually the same as the
[`path.dirname()`][] of the [`module.id`][].

#### `module.paths`
<!-- YAML
added: v0.4.0
-->

* {string[]}

The search paths for the module.

#### `module.require(id)`
<!-- YAML
added: v0.5.1
-->

* `id` {string}
* Returns: {any} exported module content

The `module.require()` method provides a way to load a module as if
`require()` was called from the original module.

In order to do this, it is necessary to get a reference to the `module` object.
Since `require()` returns the `module.exports`, and the `module` is typically
*only* available within a specific module's code, it must be explicitly exported
in order to be used.

### Tips for Package Manager Authors

<!-- type=misc -->

The semantics of the Node.js `require()` function were designed to be general
enough to support reasonable directory structures. Package manager programs
such as `dpkg`, `rpm`, and `npm` will hopefully find it possible to build
native packages from Node.js modules without modification.

Below we give a suggested directory structure that could work:

Let's say that we wanted to have the folder at
`/usr/lib/node/<some-package>/<some-version>` hold the contents of a
specific version of a package.

Packages can depend on one another. In order to install package `foo`, it
may be necessary to install a specific version of package `bar`. The `bar`
package may itself have dependencies, and in some cases, these may even collide
or form cyclic dependencies.

Since Node.js looks up the `realpath` of any modules it loads (that is,
resolves symlinks), and then looks for their dependencies in the `node_modules`
folders as described [here](#modules_loading_from_node_modules_folders), this
situation is very simple to resolve with the following architecture:

* `/usr/lib/node/foo/1.2.3/`: Contents of the `foo` package, version 1.2.3.
* `/usr/lib/node/bar/4.3.2/`: Contents of the `bar` package that `foo` depends
  on.
* `/usr/lib/node/foo/1.2.3/node_modules/bar`: Symbolic link to
  `/usr/lib/node/bar/4.3.2/`.
* `/usr/lib/node/bar/4.3.2/node_modules/*`: Symbolic links to the packages that
  `bar` depends on.

Thus, even if a cycle is encountered, or if there are dependency
conflicts, every module will be able to get a version of its dependency
that it can use.

When the code in the `foo` package does `require('bar')`, it will get the
version that is symlinked into `/usr/lib/node/foo/1.2.3/node_modules/bar`.
Then, when the code in the `bar` package calls `require('quux')`, it'll get
the version that is symlinked into
`/usr/lib/node/bar/4.3.2/node_modules/quux`.

Furthermore, to make the module lookup process even more optimal, rather
than putting packages directly in `/usr/lib/node`, we could put them in
`/usr/lib/node_modules/<name>/<version>`. Then Node.js will not bother
looking for missing dependencies in `/usr/node_modules` or `/node_modules`.

In order to make modules available to the Node.js REPL, it might be useful to
also add the `/usr/lib/node_modules` folder to the `$NODE_PATH` environment
variable. Since the module lookups using `node_modules` folders are all
relative, and based on the real path of the files making the calls to
`require()`, the packages themselves can be anywhere.

## ECMAScript modules

<!--introduced_in=v8.5.0-->

> Stability: 1 - Experimental

### `import` Specifiers

#### Terminology

The _specifier_ of an `import` statement is the string after the `from` keyword,
e.g. `'path'` in `import { sep } from 'path'`. Specifiers are also used in
`export from` statements, and as the argument to an `import()` expression.

There are four types of specifiers:

* _Bare specifiers_ like `'some-package'`. They refer to an entry point of a
  package by the package name.

* _Deep import specifiers_ like `'some-package/lib/shuffle.mjs'`. They refer to
  a path within a package prefixed by the package name.

* _Relative specifiers_ like `'./startup.js'` or `'../config.mjs'`. They refer
  to a path relative to the location of the importing file.

* _Absolute specifiers_ like `'file:///opt/nodejs/config.js'`. They refer
  directly and explicitly to a full path.

Bare specifiers, and the bare specifier portion of deep import specifiers, are
strings; but everything else in a specifier is a URL.

Only `file:` and `data:` URLs are supported. A specifier like
`'https://example.com/app.js'` may be supported by browsers but it is not
supported in Node.js.

Specifiers may not begin with `/` or `//`. These are reserved for potential
future use. The root of the current volume may be referenced via `file:///`.

##### `data:` Imports

<!-- YAML
added: v12.10.0
-->

[`data:` URLs][] are supported for importing with the following MIME types:

* `text/javascript` for ES Modules
* `application/json` for JSON
* `application/wasm` for WASM.

`data:` URLs only resolve [_Bare specifiers_][Terminology] for builtin modules
and [_Absolute specifiers_][Terminology]. Resolving
[_Relative specifiers_][Terminology] will not work because `data:` is not a
[special scheme][]. For example, attempting to load `./foo`
from `data:text/javascript,import "./foo";` will fail to resolve since there
is no concept of relative resolution for `data:` URLs. An example of a `data:`
URLs being used is:

```js
import 'data:text/javascript,console.log("hello!");';
import _ from 'data:application/json,"world!"';
```

### `import.meta`

* {Object}

The `import.meta` metaproperty is an `Object` that contains the following
property:

* `url` {string} The absolute `file:` URL of the module.

### Differences Between ES Modules and CommonJS

#### Mandatory file extensions

A file extension must be provided when using the `import` keyword. Directory
indexes (e.g. `'./startup/index.js'`) must also be fully specified.

This behavior matches how `import` behaves in browser environments, assuming a
typically configured server.

#### No `NODE_PATH`

`NODE_PATH` is not part of resolving `import` specifiers. Please use symlinks
if this behavior is desired.

#### No `require`, `exports`, `module.exports`, `__filename`, `__dirname`

These CommonJS variables are not available in ES modules.

`require` can be imported into an ES module using [`module.createRequire()`][].

Equivalents of `__filename` and `__dirname` can be created inside of each file
via [`import.meta.url`][].

```js
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
```

#### No `require.resolve`

Former use cases relying on `require.resolve` to determine the resolved path
of a module can be supported via `import.meta.resolve`, which is experimental
and supported via the `--experimental-import-meta-resolve` flag:

```js
(async () => {
  const dependencyAsset = await import.meta.resolve('component-lib/asset.css');
})();
```

`import.meta.resolve` also accepts a second argument which is the parent module
from which to resolve from:

```js
(async () => {
  // Equivalent to import.meta.resolve('./dep')
  await import.meta.resolve('./dep', import.meta.url);
})();
```

This function is asynchronous since the ES module resolver in Node.js is
asynchronous. With the introduction of [Top-Level Await][], these use cases
will be easier as they won't require an async function wrapper.

#### No `require.extensions`

`require.extensions` is not used by `import`. The expectation is that loader
hooks can provide this workflow in the future.

#### No `require.cache`

`require.cache` is not used by `import`. It has a separate cache.

#### URL-based paths

ES modules are resolved and cached based upon
[URL](https://url.spec.whatwg.org/) semantics. This means that files containing
special characters such as `#` and `?` need to be escaped.

Modules will be loaded multiple times if the `import` specifier used to resolve
them have a different query or fragment.

```js
import './foo.mjs?query=1'; // loads ./foo.mjs with query of "?query=1"
import './foo.mjs?query=2'; // loads ./foo.mjs with query of "?query=2"
```

For now, only modules using the `file:` protocol can be loaded.

### Interoperability with CommonJS

#### `require`

`require` always treats the files it references as CommonJS. This applies
whether `require` is used the traditional way within a CommonJS environment, or
in an ES module environment using [`module.createRequire()`][].

To include an ES module into CommonJS, use [`import()`][].

#### `import` statements

An `import` statement can reference an ES module or a CommonJS module. Other
file types such as JSON or Native modules are not supported. For those, use
[`module.createRequire()`][].

`import` statements are permitted only in ES modules. For similar functionality
in CommonJS, see [`import()`][].

The _specifier_ of an `import` statement (the string after the `from` keyword)
can either be an URL-style relative path like `'./file.mjs'` or a package name
like `'fs'`.

Like in CommonJS, files within packages can be accessed by appending a path to
the package name; unless the package’s `package.json` contains an `"exports"`
field, in which case files within packages need to be accessed via the path
defined in `"exports"`.

```js
import { sin, cos } from 'geometry/trigonometry-functions.mjs';
```

Only the “default export” is supported for CommonJS files or packages:

<!-- eslint-disable no-duplicate-imports -->
```js
import packageMain from 'commonjs-package'; // Works

import { method } from 'commonjs-package'; // Errors
```

It is also possible to
[import an ES or CommonJS module for its side effects only][].

#### `import()` expressions

[Dynamic `import()`][] is supported in both CommonJS and ES modules. It can be
used to include ES module files from CommonJS code.

### CommonJS, JSON, and Native Modules

CommonJS, JSON, and Native modules can be used with
[`module.createRequire()`][].

```js
// cjs.cjs
module.exports = 'cjs';

// esm.mjs
import { createRequire } from 'module';

const require = createRequire(import.meta.url);

const cjs = require('./cjs.cjs');
cjs === 'cjs'; // true
```

### Builtin modules

Builtin modules will provide named exports of their public API. A
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

fs.readFileSync = () => Buffer.from('Hello, ESM');
syncBuiltinESMExports();

fs.readFileSync === readFileSync;
```

### Experimental JSON Modules

Currently importing JSON modules are only supported in the `commonjs` mode
and are loaded using the CJS loader. [WHATWG JSON modules specification][] are
still being standardized, and are experimentally supported by including the
additional flag `--experimental-json-modules` when running Node.js.

When the `--experimental-json-modules` flag is included both the
`commonjs` and `module` mode will use the new experimental JSON
loader. The imported JSON only exposes a `default`, there is no
support for named exports. A cache entry is created in the CommonJS
cache, to avoid duplication. The same object will be returned in
CommonJS if the JSON module has already been imported from the
same path.

Assuming an `index.mjs` with

```js
import packageConfig from './package.json';
```

The `--experimental-json-modules` flag is needed for the module
to work.

```bash
node index.mjs # fails
node --experimental-json-modules index.mjs # works
```

### Experimental Wasm Modules

Importing Web Assembly modules is supported under the
`--experimental-wasm-modules` flag, allowing any `.wasm` files to be
imported as normal modules while also supporting their module imports.

This integration is in line with the
[ES Module Integration Proposal for Web Assembly][].

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

### Experimental Top-Level `await`

When the `--experimental-top-level-await` flag is provided, `await` may be used
in the top level (outside of async functions) within modules. This implements
the [ECMAScript Top-Level `await` proposal][].

Assuming an `a.mjs` with

<!-- eslint-skip -->
```js
export const five = await Promise.resolve(5);
```

And a `b.mjs` with

```js
import { five } from './a.mjs';

console.log(five); // Logs `5`
```

```bash
node b.mjs # fails
node --experimental-top-level-await b.mjs # works
```

### Experimental Loaders

**Note: This API is currently being redesigned and will still change.**

<!-- type=misc -->

To customize the default module resolution, loader hooks can optionally be
provided via a `--experimental-loader ./loader-name.mjs` argument to Node.js.

When hooks are used they only apply to ES module loading and not to any
CommonJS modules loaded.

#### Hooks

##### <code>resolve</code> hook

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

The `resolve` hook returns the resolved file URL for a given module specifier
and parent URL. The module specifier is the string in an `import` statement or
`import()` expression, and the parent URL is the URL of the module that imported
this one, or `undefined` if this is the main entry point for the application.

The `conditions` property on the `context` is an array of conditions for
[Conditional Exports][] that apply to this resolution request. They can be used
for looking up conditional mappings elsewhere or to modify the list when calling
the default resolution logic.

The [current set of Node.js default conditions][Conditional Exports] will always
be in the `context.conditions` list passed to the hook. If the hook wants to
ensure Node.js-compatible resolution logic, all items from this default
condition list **must** be passed through to the `defaultResolve` function.

```js
/**
 * @param {string} specifier
 * @param {object} context
 * @param {string} context.parentURL
 * @param {string[]} context.conditions
 * @param {function} defaultResolve
 * @returns {object} response
 * @returns {string} response.url
 */
export async function resolve(specifier, context, defaultResolve) {
  const { parentURL = null } = context;
  if (someCondition) {
    // For some or all specifiers, do some custom logic for resolving.
    // Always return an object of the form {url: <string>}
    return {
      url: (parentURL) ?
        new URL(specifier, parentURL).href : new URL(specifier).href
    };
  }
  if (anotherCondition) {
    // When calling the defaultResolve, the arguments can be modified. In this
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

##### <code>getFormat</code> hook

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

The `getFormat` hook provides a way to define a custom method of determining how
a URL should be interpreted. The `format` returned also affects what the
acceptable forms of source values are for a module when parsing. This can be one
of the following:

| `format` | Description | Acceptable Types For `source` Returned by `getSource` or `transformSource` |
| --- | --- | --- |
| `'builtin'` | Load a Node.js builtin module | Not applicable |
| `'commonjs'` | Load a Node.js CommonJS module | Not applicable |
| `'dynamic'` | Use a [dynamic instantiate hook][] | Not applicable |
| `'json'` | Load a JSON file | { [ArrayBuffer][], [string][], [TypedArray][] } |
| `'module'` | Load an ES module | { [ArrayBuffer][], [string][], [TypedArray][] } |
| `'wasm'` | Load a WebAssembly module | { [ArrayBuffer][], [string][], [TypedArray][] } |

Note: These types all correspond to classes defined in ECMAScript.

* The specific [ArrayBuffer][] object is a [SharedArrayBuffer][].
* The specific [string][] object is not the class constructor, but an instance.
* The specific [TypedArray][] object is a [Uint8Array][].

Note: If the source value of a text-based format (i.e., `'json'`, `'module'`) is
not a string, it will be converted to a string using [`util.TextDecoder`][].

```js
/**
 * @param {string} url
 * @param {object} context (currently empty)
 * @param {function} defaultGetFormat
 * @returns {object} response
 * @returns {string} response.format
 */
export async function getFormat(url, context, defaultGetFormat) {
  if (someCondition) {
    // For some or all URLs, do some custom logic for determining format.
    // Always return an object of the form {format: <string>}, where the
    // format is one of the strings in the table above.
    return {
      format: 'module'
    };
  }
  // Defer to Node.js for all other URLs.
  return defaultGetFormat(url, context, defaultGetFormat);
}
```

##### <code>getSource</code> hook

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

The `getSource` hook provides a way to define a custom method for retrieving
the source code of an ES module specifier. This would allow a loader to
potentially avoid reading files from disk.

```js
/**
 * @param {string} url
 * @param {object} context
 * @param {string} context.format
 * @param {function} defaultGetSource
 * @returns {object} response
 * @returns {string|buffer} response.source
 */
export async function getSource(url, context, defaultGetSource) {
  const { format } = context;
  if (someCondition) {
    // For some or all URLs, do some custom logic for retrieving the source.
    // Always return an object of the form {source: <string|buffer>}.
    return {
      source: '...'
    };
  }
  // Defer to Node.js for all other URLs.
  return defaultGetSource(url, context, defaultGetSource);
}
```

##### <code>transformSource</code> hook

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

The `transformSource` hook provides a way to modify the source code of a loaded
ES module file after the source string has been loaded but before Node.js has
done anything with it.

If this hook is used to convert unknown-to-Node.js file types into executable
JavaScript, a resolve hook is also necessary in order to register any
unknown-to-Node.js file extensions. See the [transpiler loader example][] below.

```js
/**
 * @param {string|buffer} source
 * @param {object} context
 * @param {string} context.url
 * @param {string} context.format
 * @param {function} defaultTransformSource
 * @returns {object} response
 * @returns {string|buffer} response.source
 */
export async function transformSource(source,
                                      context,
                                      defaultTransformSource) {
  const { url, format } = context;
  if (someCondition) {
    // For some or all URLs, do some custom logic for modifying the source.
    // Always return an object of the form {source: <string|buffer>}.
    return {
      source: '...'
    };
  }
  // Defer to Node.js for all other sources.
  return defaultTransformSource(
    source, context, defaultTransformSource);
}
```

##### <code>getGlobalPreloadCode</code> hook

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

Sometimes it can be necessary to run some code inside of the same global scope
that the application will run in. This hook allows to return a string that will
be ran as sloppy-mode script on startup.

Similar to how CommonJS wrappers work, the code runs in an implicit function
scope. The only argument is a `require`-like function that can be used to load
builtins like "fs": `getBuiltin(request: string)`.

If the code needs more advanced `require` features, it will have to construct
its own `require` using  `module.createRequire()`.

```js
/**
 * @returns {string} Code to run before application startup
 */
export function getGlobalPreloadCode() {
  return `\
globalThis.someInjectedProperty = 42;
console.log('I just set some globals!');

const { createRequire } = getBuiltin('module');

const require = createRequire(process.cwd() + '/<preload>');
// [...]
`;
}
```

##### <code>dynamicInstantiate</code> hook

> Note: The loaders API is being redesigned. This hook may disappear or its
> signature may change. Do not rely on the API described below.

To create a custom dynamic module that doesn't correspond to one of the
existing `format` interpretations, the `dynamicInstantiate` hook can be used.
This hook is called only for modules that return `format: 'dynamic'` from
the [`getFormat` hook][].

```js
/**
 * @param {string} url
 * @returns {object} response
 * @returns {array} response.exports
 * @returns {function} response.execute
 */
export async function dynamicInstantiate(url) {
  return {
    exports: ['customExportName'],
    execute: (exports) => {
      // Get and set functions provided for pre-allocated export names
      exports.customExportName.set('value');
    }
  };
}
```

With the list of module exports provided upfront, the `execute` function will
then be called at the exact point of module evaluation order for that module
in the import tree.

#### Examples

The various loader hooks can be used together to accomplish wide-ranging
customizations of Node.js’ code loading and evaluation behaviors.

##### HTTPS loader

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

export function getFormat(url, context, defaultGetFormat) {
  // This loader assumes all network-provided JavaScript is ES module code.
  if (url.startsWith('https://')) {
    return {
      format: 'module'
    };
  }

  // Let Node.js handle all other URLs.
  return defaultGetFormat(url, context, defaultGetFormat);
}

export function getSource(url, context, defaultGetSource) {
  // For JavaScript to be loaded over the network, we need to fetch and
  // return it.
  if (url.startsWith('https://')) {
    return new Promise((resolve, reject) => {
      get(url, (res) => {
        let data = '';
        res.on('data', (chunk) => data += chunk);
        res.on('end', () => resolve({ source: data }));
      }).on('error', (err) => reject(err));
    });
  }

  // Let Node.js handle all other URLs.
  return defaultGetSource(url, context, defaultGetSource);
}
```

```js
// main.mjs
import { VERSION } from 'https://coffeescript.org/browser-compiler-modern/coffeescript.js';

console.log(VERSION);
```

With this loader, running:

```console
node --experimental-loader ./https-loader.mjs ./main.js
```

Will print the current version of CoffeeScript per the module at the URL in
`main.mjs`.

##### Transpiler loader

Sources that are in formats Node.js doesn’t understand can be converted into
JavaScript using the [`transformSource` hook][]. Before that hook gets called,
however, other hooks need to tell Node.js not to throw an error on unknown file
types; and to tell Node.js how to load this new file type.

This is less performant than transpiling source files before running
Node.js; a transpiler loader should only be used for development and testing
purposes.

```js
// coffeescript-loader.mjs
import { URL, pathToFileURL } from 'url';
import CoffeeScript from 'coffeescript';

const baseURL = pathToFileURL(`${process.cwd()}/`).href;

// CoffeeScript files end in .coffee, .litcoffee or .coffee.md.
const extensionsRegex = /\.coffee$|\.litcoffee$|\.coffee\.md$/;

export function resolve(specifier, context, defaultResolve) {
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

export function getFormat(url, context, defaultGetFormat) {
  // Now that we patched resolve to let CoffeeScript URLs through, we need to
  // tell Node.js what format such URLs should be interpreted as. For the
  // purposes of this loader, all CoffeeScript URLs are ES modules.
  if (extensionsRegex.test(url)) {
    return {
      format: 'module'
    };
  }

  // Let Node.js handle all other URLs.
  return defaultGetFormat(url, context, defaultGetFormat);
}

export function transformSource(source, context, defaultTransformSource) {
  const { url, format } = context;

  if (extensionsRegex.test(url)) {
    return {
      source: CoffeeScript.compile(source, { bare: true })
    };
  }

  // Let Node.js handle all other sources.
  return defaultTransformSource(source, context, defaultTransformSource);
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

With this loader, running:

```console
node --experimental-loader ./coffeescript-loader.mjs main.coffee
```

Will cause `main.coffee` to be turned into JavaScript after its source code is
loaded from disk but before Node.js executes it; and so on for any `.coffee`,
`.litcoffee` or `.coffee.md` files referenced via `import` statements of any
loaded file.

## Utility Methods

<!-- YAML
added: v0.3.7
-->

* {Object}

Provides general utility methods when interacting with instances of
`Module`, the `module` variable often seen in file modules. Accessed
via `require('module')`.

### `module.builtinModules`
<!-- YAML
added:
  - v9.3.0
  - v8.10.0
  - v6.13.0
-->

* {string[]}

A list of the names of all modules provided by Node.js. Can be used to verify
if a module is maintained by a third party or not.

`module` in this context isn't the same object that's provided
by the [module wrapper][]. To access it, require the `Module` module:

```js
const builtin = require('module').builtinModules;
```

### `module.createRequire(filename)`
<!-- YAML
added: v12.2.0
-->

* `filename` {string|URL} Filename to be used to construct the require
  function. Must be a file URL object, file URL string, or absolute path
  string.
* Returns: {require} Require function

```js
import { createRequire } from 'module';
const require = createRequire(import.meta.url);

// sibling-module.js is a CommonJS module.
const siblingModule = require('./sibling-module');
```

### `module.createRequireFromPath(filename)`
<!-- YAML
added: v10.12.0
deprecated: v12.2.0
-->

> Stability: 0 - Deprecated: Please use [`createRequire()`][] instead.

* `filename` {string} Filename to be used to construct the relative require
  function.
* Returns: {require} Require function

```js
const { createRequireFromPath } = require('module');
const requireUtil = createRequireFromPath('../src/utils/');

// Require `../src/utils/some-tool`
requireUtil('./some-tool');
```

### `module.syncBuiltinESMExports()`
<!-- YAML
added: v12.12.0
-->

The `module.syncBuiltinESMExports()` method updates all the live bindings for
builtin ES Modules to match the properties of the CommonJS exports. It does
not add or remove exported names from the ES Modules.

```js
const fs = require('fs');
const { syncBuiltinESMExports } = require('module');

fs.readFile = null;

delete fs.readFileSync;

fs.newAPI = function newAPI() {
  // ...
};

syncBuiltinESMExports();

import('fs').then((esmFS) => {
  assert.strictEqual(esmFS.readFile, null);
  assert.strictEqual('readFileSync' in fs, true);
  assert.strictEqual(esmFS.newAPI, undefined);
});
```

## Source Map V3 Support
<!-- YAML
added: v13.7.0
-->

> Stability: 1 - Experimental

Helpers for interacting with the source map cache. This cache is
populated when source map parsing is enabled and
[source map include directives][] are found in a modules' footer.

To enable source map parsing, Node.js must be run with the flag
[`--enable-source-maps`][], or with code coverage enabled by setting
[`NODE_V8_COVERAGE=dir`][].

```js
const { findSourceMap, SourceMap } = require('module');
```

### `module.findSourceMap(path[, error])`
<!-- YAML
added: v13.7.0
-->

* `path` {string}
* `error` {Error}
* Returns: {module.SourceMap}

`path` is the resolved path for the file for which a corresponding source map
should be fetched.

The `error` instance should be passed as the second parameter to `findSourceMap`
in exceptional flows, e.g., when an overridden
[`Error.prepareStackTrace(error, trace)`][] is invoked. Modules are not added to
the module cache until they are successfully loaded, in these cases source maps
will be associated with the `error` instance along with the `path`.

### Class: `module.SourceMap`
<!-- YAML
added: v13.7.0
-->

#### `new SourceMap(payload)`

* `payload` {Object}

Creates a new `sourceMap` instance.

`payload` is an object with keys matching the [Source Map V3 format][]:

* `file`: {string}
* `version`: {number}
* `sources`: {string[]}
* `sourcesContent`: {string[]}
* `names`: {string[]}
* `mappings`: {string}
* `sourceRoot`: {string}

#### `sourceMap.payload`

* Returns: {Object}

Getter for the payload used to construct the [`SourceMap`][] instance.

#### `sourceMap.findEntry(lineNumber, columnNumber)`

* `lineNumber` {number}
* `columnNumber` {number}
* Returns: {Object}

Given a line number and column number in the generated source file, returns
an object representing the position in the original file. The object returned
consists of the following keys:

* generatedLine: {number}
* generatedColumn: {number}
* originalSource: {string}
* originalLine: {number}
* originalColumn: {number}

## Resolution Algorithms

### CommonJS Modules Resolution Algorithm

<!-- type=misc -->

To get the exact filename that will be loaded when `require()` is called, use
the `require.resolve()` function.

Putting together all of the above, here is the high-level algorithm
in pseudocode of what `require()` does:

```txt
require(X) from module at path Y
1. If X is a core module,
   a. return the core module
   b. STOP
2. If X begins with '/'
   a. set Y to be the filesystem root
3. If X begins with './' or '/' or '../'
   a. LOAD_AS_FILE(Y + X)
   b. LOAD_AS_DIRECTORY(Y + X)
   c. THROW "not found"
4. LOAD_SELF_REFERENCE(X, dirname(Y))
5. LOAD_NODE_MODULES(X, dirname(Y))
6. THROW "not found"

LOAD_AS_FILE(X)
1. If X is a file, load X as its file extension format.  STOP
2. If X.js is a file, load X.js as JavaScript text.  STOP
3. If X.json is a file, parse X.json to a JavaScript Object.  STOP
4. If X.node is a file, load X.node as binary addon.  STOP

LOAD_INDEX(X)
1. If X/index.js is a file, load X/index.js as JavaScript text.  STOP
2. If X/index.json is a file, parse X/index.json to a JavaScript object. STOP
3. If X/index.node is a file, load X/index.node as binary addon.  STOP

LOAD_AS_DIRECTORY(X)
1. If X/package.json is a file,
   a. Parse X/package.json, and look for "main" field.
   b. If "main" is a falsy value, GOTO 2.
   c. let M = X + (json main field)
   d. LOAD_AS_FILE(M)
   e. LOAD_INDEX(M)
   f. LOAD_INDEX(X) DEPRECATED
   g. THROW "not found"
2. LOAD_INDEX(X)

LOAD_NODE_MODULES(X, START)
1. let DIRS = NODE_MODULES_PATHS(START)
2. for each DIR in DIRS:
   a. LOAD_PACKAGE_EXPORTS(DIR, X)
   b. LOAD_AS_FILE(DIR/X)
   c. LOAD_AS_DIRECTORY(DIR/X)

NODE_MODULES_PATHS(START)
1. let PARTS = path split(START)
2. let I = count of PARTS - 1
3. let DIRS = [GLOBAL_FOLDERS]
4. while I >= 0,
   a. if PARTS[I] = "node_modules" CONTINUE
   b. DIR = path join(PARTS[0 .. I] + "node_modules")
   c. DIRS = DIRS + DIR
   d. let I = I - 1
5. return DIRS

LOAD_SELF_REFERENCE(X, START)
1. Find the closest package scope to START.
2. If no scope was found, return.
3. If the `package.json` has no "exports", return.
4. If the name in `package.json` isn't a prefix of X, throw "not found".
5. Otherwise, load the remainder of X relative to this package as if it
  was loaded via `LOAD_NODE_MODULES` with a name in `package.json`.

LOAD_PACKAGE_EXPORTS(DIR, X)
1. Try to interpret X as a combination of name and subpath where the name
   may have a @scope/ prefix and the subpath begins with a slash (`/`).
2. If X does not match this pattern or DIR/name/package.json is not a file,
   return.
3. Parse DIR/name/package.json, and look for "exports" field.
4. If "exports" is null or undefined, return.
5. If "exports" is an object with some keys starting with "." and some keys
  not starting with ".", throw "invalid config".
6. If "exports" is a string, or object with no keys starting with ".", treat
  it as having that value as its "." object property.
7. If subpath is "." and "exports" does not have a "." entry, return.
8. Find the longest key in "exports" that the subpath starts with.
9. If no such key can be found, throw "not found".
10. let RESOLVED =
    fileURLToPath(PACKAGE_EXPORTS_TARGET_RESOLVE(pathToFileURL(DIR/name),
    exports[key], subpath.slice(key.length), ["node", "require"])), as defined
    in the ESM resolver.
11. If key ends with "/":
    a. LOAD_AS_FILE(RESOLVED)
    b. LOAD_AS_DIRECTORY(RESOLVED)
12. Otherwise
   a. If RESOLVED is a file, load it as its file extension format.  STOP
13. Throw "not found"
```

[GLOBAL_FOLDERS]: #modules_loading_from_the_global_folders
[`Error`]: errors.html#errors_class_error
[`__dirname`]: #modules_dirname
[`__filename`]: #modules_filename
[`createRequire()`]: #modules_module_createrequire_filename
[`module` object]: #modules_utility_methods
[`module.id`]: #modules_module_id
[`path.dirname()`]: path.html#path_path_dirname_path
[ECMAScript Modules]: esm.html
[an error]: errors.html#errors_err_require_esm
[exports shortcut]: #modules_exports_shortcut
[module resolution]: #modules_resolution_algorithms
[module wrapper]: #modules_the_module_wrapper
[native addons]: addons.html
[source map include directives]: https://sourcemaps.info/spec.html#h.lmz475t4mvbx
[`--enable-source-maps`]: cli.html#cli_enable_source_maps
[`NODE_V8_COVERAGE=dir`]: cli.html#cli_node_v8_coverage_dir
[`Error.prepareStackTrace(error, trace)`]: https://v8.dev/docs/stack-trace-api#customizing-stack-traces
[`SourceMap`]: modules.html#modules_class_module_sourcemap
[Source Map V3 format]: https://sourcemaps.info/spec.html#h.mofvlxcwqzej
