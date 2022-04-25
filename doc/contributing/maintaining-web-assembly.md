# Maintaining WebAssembly

Support for [WebAssembly](https://webassembly.org/)
has been identified as one of the
[top technical priorities](https://github.com/nodejs/node/blob/master/doc/contributing/technical-priorities.md#webassembly)
for the future success of Node.js.

This document provides an overview of our high-level strategy for
supporting WebAssembly and information about our current implementation
as a starting point for contributors.

## High-level approach

The key elements of our WebAssembly strategy include:

* Up-to-date core WebAssembly support
* Support for high-level APIs
* Making it easy to load WebAssembly
* Making sure the core Node.js APIs are compatible with WebAssembly
  and can be called in an efficient manner from WebAssembly

### Up-to-date core WebAssembly support

Node.js gets its core WebAssembly support through V8. We don't need
to do anything specific to support this, all we have to do is keep
the version of V8 as up-to-date as possible.

### Key API support

As a runtime, Node.js must implement a number of APIs in addition
to the core WebAssembly support in order to be a good choice to run
WebAssembly. The project has currently identified these additional
APIs as important:

* WebAssembly System Interface (WASI). This provides the ability for
  WebAssembly to interact with the outside world. Node.js currently
  has an implementation (see below for more details).
* [WebAssembly Web API](https://www.w3.org/TR/wasm-web-api-1/). Node.js
  currently has an implementation of streaming module compilation and
  instantiation. As this and other specifications evolve, keeping up with them
  will be important.
* [WebAssembly Component Model](https://github.com/WebAssembly/component-model/).
  This API is still in the definition stage but the project should
  keep track of its development as a way to simplify native code
  integration.

### Making it as easy as possible to load WASM

The most important thing we can do on this front is to either find and
reference resources or provide resources on how to:

* Compile your WebAssembly code (outside of Node.js) and integrate that
  into an npm workflow.
* Load and run WebAssembly code in your Node.js application.

It is also important to support and track the ongoing work in ESM to enable
loading of WebAssembly with ESM.

### Making sure the core Node.js APIs are compatible with WebAssembly

Use cases for which Node.js will be a good runtime will include code
both in JavaScript and compiled into WebAssembly. It is important
that Node.js APIs are able to be called from WebAssembly in
an efficient manner without extra buffer copies. We need to:

* Review APIs and identify those that can be called often from
  WebAssembly.
* Where appropriate, make additions to identified APIs to allow
  a pre-existing buffer to be passed in order to avoid copies.

## Current implementation and assets

### WebAssembly System Interface (WASI)

The Node.js WASI implementation is maintained in the
[uvwasi](https://github.com/nodejs/uvwasi) repository in the
Node.js GitHub organization. As needed, an updated copy
is vendored into the Node.js deps in
[deps/uvwasi](https://github.com/nodejs/node/tree/master/deps/uvwasi).

To update the copy of uvwasi in the Node.js deps:

* Copy over the contents of `include` and `src` to the corresponding
  directories.
* Check if any additional files have been added and need to be added
  to the `sources` list in `deps/uvwasi/uvwasi.gyp`.

In addition to the code from uvwasi, Node.js includes bindings and
APIs that allow WebAssembly to be run with WASI support from Node.js.
The documentation for this API is in
[WebAssembly System Interface (WASI)](https://nodejs.org/api/wasi.html).

The implementation of the bindings and the public API is in:

* [src/node\_wasi.h](https://github.com/nodejs/node/blob/master/src/node_wasi.h)
* [src/node\_wasi.cc](https://github.com/nodejs/node/blob/master/src/node_wasi.cc)
* [lib/wasi.js](https://github.com/nodejs/node/blob/master/lib/wasi.js)
