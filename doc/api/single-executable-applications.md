# Single executable applications

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental: This feature is currently being designed and will
> still change.

<!-- source_link=lib/internal/main/single_executable_application.js -->

Node.js supports the creation of [single executable applications][] by allowing
the injection of a JavaScript file into the `node` binary. During start up, the
program checks if a resource (on [PE][]) or section (on [Mach-O][]) or note (on
[ELF][]) named `NODE_JS_CODE` exists (on macOS, in the `NODE_JS` segment). If it
is found, it executes its contents, otherwise it operates like plain Node.js.

This feature allows the distribution of a Node.js application conveniently to a
system that does not have Node.js installed.

A bundled JavaScript file can be turned into a single executable application
with any other tool which can inject resources into the Node.js binary. The tool
should also search the binary for the
`NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0` fuse string and flip the last
character to `1` to indicate that a resource has been injected.

One such tool is [postject][]:

```console
$ cat hello.js
console.log(`Hello, ${process.argv[2]}!`);
$ cp $(command -v node) hello
$ npx postject hello NODE_JS_CODE hello.js \
    --sentinel-fuse NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2 # Also add `--macho-segment-name NODE_JS` on macOS.
$ ./hello world
Hello, world!
```

This currently only supports running a single embedded [CommonJS][] file.

## Notes

### `require(id)` in the injected module is not file based

This is not the same as [`require()`][]. This also does not have any of the
properties that [`require()`][] has except [`require.main`][]. It is used to
import only built-in modules. Attempting to import a module available on the
file system will throw an error.

Since the injected JavaScript file would be bundled into a standalone module by
default in most cases, there shouldn't be any need for a file based `require()`
API. Not having a file based `require()` API in the single-executable
application should also safeguard users from some security vulnerabilities.

However, if a file based `require()` is still needed, that can also be achieved:

```js
const { createRequire } = require('node:module');
require = createRequire(__filename);
```

### `__filename` and `module.filename` in the injected module

The values of `__filename` and `module.filename` in the injected module are
equal to [`process.execPath`][].

### `__dirname` in the injected module

The value of `__dirname` in the injected module is equal to the directory name
of [`process.execPath`][].

[CommonJS]: modules.md#modules-commonjs-modules
[ELF]: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
[Mach-O]: https://en.wikipedia.org/wiki/Mach-O
[PE]: https://en.wikipedia.org/wiki/Portable_Executable
[`process.execPath`]: process.md#processexecpath
[`require()`]: modules.md#requireid
[`require.main`]: modules.md#accessing-the-main-module
[postject]: https://github.com/nodejs/postject
[single executable applications]: https://github.com/nodejs/single-executable
