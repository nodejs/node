# Node Core's Internal Structure

This document covers the basics of how Node.js is structured, in an attempt to
make it easier to contribute to.

Note: The Node.js codebase is in a constant amount of flux, things can and will
change over time, and so also parts of this document will go out of date over
time.


## Top-Level File & Folder Structure

At the root of Node.js you have a set of files and folders that can be roughly
identified as the following:

- CAPITALS documentation files
- Documentation (`doc/`)
- Top-level Build Tooling
- JavaScript "standard" Library (`lib/`)
- C++ "native" Source (`src/`)
- Tests & Benchmarks (`test/` & `benchmark/`)
- Internal / Build Tooling (`tools/`)
- Dependencies (`deps/`)
- .dotfiles

Below is a summary of the parts, ordered as per the list above.


### CAPITALS files

These are pretty important, and describe things such as our [GOVERNANCE][], [CONTRIBUTING][], and [CODE_OF_CONDUCT][] policies.
The [LICENSE][] is also part of these.

If you plan on building node, it is recommended you check out the [BUILDING][]
document.

Typically these are only found at the root directory of the project.


### Documentation (`doc/`)

All the bundled documentation assets, including the [API docs][] that are
available on the Nodejs.org website, can be found within the `doc/` folder.

The API docs are constructed from the markdown files within the `doc/api/`
folder.

The documentation also includes topic documents such as this one such as this,
how-to guides, the `node.1` [manual page][], and an archive of Technical
Committee meeting notes.


### Top-level Build Tooling

If you are planning on making modifications to Node.js, you will need to be able
to build the `node` binary.
Full instructions can be found in the [BUILDING][] document.

The top-level (root directory) build files help you achieve this, most notably
[`configure`][] & [`Makefile`][] for UNIX systems and [`vcbuild.bat`][] for
Windows.

There are also some other build files in the root directory, such as the
`node.gyp` gyp build configuration. Though in most cases, those will not need to
be edited.


### JavaScript Library (`lib/`)

The JavaScript library found within the `lib/` folder is one of the most
important parts of Node.js, and you will most likely be interacting with the
files within if you are making code modifications.

All of node's exposed API can be found exported from these files.
Specifically, any `.js` file directly within the `lib/` folder is exported as a
module that can be `require()`'d in user code.
Node.js also has some internal files, for utilities not intended to be exposed.
Those can be found in the `lib/internal/` folder, and files within are not
accessible to JavaScript code outside of the node core.

Additionally, Node.js's own JavaScript entry point can be found at
`lib/internal/bootstrap_node.js`. This file determines how to execute the user
code, and also helps set up the `process` global object, importing lots from
`lib/internal/process/`.

As you inspect these files, you will occasionally see code that calls
`process.binding()` to import additional code or data. This leads us into the
next segment...


### C++ Source (`src/`)

Node.js is built ontop of a C++ base for both bootstrapping and performance
reasons. This also helps for interoperability with Node.js's underlying
JavaScript engine, V8, which is also written in C++.

Some of the C++ source files export JavaScript code defined in C++ via the V8
API with the help of internal node helpers. These files are the "glue" between
the JavaScript & C++ layers, and often times act as a proxy between JS and
Libuv, the I/O library written in C which node builds atop of. The files which
are exposed to `process.binding()` all do so via a
`NODE_MODULE_CONTEXT_AWARE_BUILTIN` Macro, and are able to be found by doing a
search of the folder.

The contents of the `src/` folder can generalized into 3 parts:
- `node_<subsystem>` files (Usually helpers exposed to JS)
- `<subsystem>_wrap` files (Libuv wrappers for Proxying to/from JS APIs)
- Other bootstrapping / utility `.cc`/`.h` files, particularly `node.cc` and `env-inl.h`.

The following traces Node.js through startup, showing how it interacts with
C++ & JS:

Step | Description | Source Location
---|---|---
1 | startup: `main --> node::Start --> node::StartNodeInstance` | ./src/node_main.cc::51, ./src/node.cc::4345, ./src/node.cc::4383
2 | create JSVM (`v8::isolate`) | ./src/node.cc::4372, ./src/node.cc::4261
3 | create event loop (`uv_loop_t`) | ./src/node.cc::4377
4 | create native environment (`node::Environment`) containing JSVM and event loop | ./src/node.cc::4276, ./src/node.cc::4181
5 | create JS `process` object bound to native environment | ./src/node.cc::4237-4244
6 | initialize JS context and `process` object through JS script | ./src/node.cc::4289, ./src/node.cc::3330, ./lib/internal/bootstrap_node.js
7 | invoke user script (or repl, --eval, etc.) | ./lib/internal/bootstrap_node.js::66


### Tests & Benchmarks (`test/` & `benchmark/`)

Node.js's extensive test suite can be found within `test/`. Most new & existing
tests can be found within `test/parallel/`, which is the directory we tell the
test runner to parallelize over so that the test suite can run faster.

Most times if you modify code within node, tests for that behavior will also be
needed for the change to be accepted.

The tests can be run via `make -j4 test` on Unix, or `vcbuild test` on Windows.
Do note that Node.js will have to be built before. Please see [BUILDING][] for
details on the build prerequisites.

Additionally, Node.js also contains a benchmarks suite for performance-critical
areas. Typically, new contributions do not require new benchmarks unless they
are performance-specific changes. At the time of writing, the benchmarks suite
is far from comprehensive and is in need of improvement.


### Build / Internal Tooling (`tools/`)

These are mostly contained within the `tools/` folder, but some tools files,
mentioned previously, are top-level so they can be easily run from the root
directory. If you are building node you will become quickly familiar with some
of these.

Other files here include the various gyp and native JavaScript embedding tools.
Some of this is quite complex and specific in order to build V8 correctly, and
also let node run on a vast array of different platforms. Lots of these are
quite arcane in nature.

The [gyp][] and [eslint][] tooling dependencies also live in the `tools/`
folder. Patches to these must be sent to their upstream projects like the rest
of our dependencies, as outlined in the section below.


### Dependencies (`deps/`)

Node.js is built on a number of dependencies that allow it to function. Most of
these are "native" (C/C++), and must be compiled.

Node Core does not directly take patches or pull requests to most of its
dependencies and instead prefers patches to be "upstreamed" to the origin
repository.

Following is a list of the dependencies in `deps/` and their repos:

- c-ares: https://github.com/c-ares/c-ares
- gtest: Comes from v8, see below.
- http_parser: https://github.com/nodejs/http-parser
- icu-small: http://site.icu-project.org/repository
- npm: https://github.com/npm/npm
- OpenSSL: https://github.com/openssl/openssl
- libuv (`uv`): https://github.com/libuv/libuv
- v8: https://chromium.googlesource.com/v8/v8.git & https://bugs.chromium.org/p/v8/issues/list
- zlib: https://github.com/madler/zlib


### .dotfiles

These contain the git information, automatic style settings for most text
editors (`.editorconfig`), and also JavaScript lint configurations for eslint.


<!-- Repository Files -->
[BUILDING]: ../../BUILDING.md
[CODE_OF_CONDUCT]: ../../CODE_OF_CONDUCT.md
[`configure`]: ../../configure
[CONTRIBUTING]: ../../CONTRIBUTING.md
[GOVERNANCE]: ../../GOVERNANCE.md
[LICENSE]: ../../LICENSE
[`Makefile`]: ../../Makefile
[`vcbuild.bat`]: ../../vcbuild.bat

<!-- External Links -->
[API docs]: https://nodejs.org/api/
[eslint]: https://github.com/eslint/eslint
[gyp]: https://chromium.googlesource.com/external/gyp
[manual page]: https://en.wikipedia.org/wiki/Man_page
