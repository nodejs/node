# Maintaining cjs-module-lexer

The [cjs-module-lexer](https://github.com/nodejs/node/tree/HEAD/deps/cjs-module-lexer)
dependency is used within the Node.js ESM implementation to detect the
named exports of a CommonJS module.

It is used within
[`node:internal/modules/esm/translators`](https://github.com/nodejs/node/blob/HEAD/lib/internal/modules/esm/translators.js)
in which both `internal/deps/cjs-module-lexer/lexer` and
`internal/deps/cjs-module-lexer/dist/lexer` are required and used.

`internal/deps/cjs-module-lexer/lexer`
is a regular JavaScript implementation that is
used when WebAssembly is not available on a platform.
`internal/deps/cjs-module-lexer/dist/lexer` is a faster
implementation using WebAssembly which is generated from a
C based implementation. These two paths
resolve to the files in `deps/cjs-module-lexer` due to their
inclusion in the `deps_files` entry in
[node.gyp](https://github.com/nodejs/node/blob/main/node.gyp).

The two different versions of lexer.js are maintained in the
[nodejs/cjs-module-lexer][] project.

In order to update the Node.js dependencies to use to a newer version
of cjs-module-lexer, complete the following steps:

* Clone [nodejs/cjs-module-lexer][]
  and check out the version that you want Node.js to use.
* Follow the WASM build steps outlined in
  [wasm-build-steps](https://github.com/nodejs/cjs-module-lexer#wasm-build-steps).
  This will generate the WASM based dist/lexer.js file.
* Preserving the same directory structure, copy the following files over
  to `deps/cjs-module-lexer` directory where you have checked out Node.js.

```text
├── CHANGELOG.md
├── dist
│   ├── lexer.js
│   └── lexer.mjs
├── lexer.js
├── LICENSE
├── package.json
└── README.md
```

* Update the link to the cjs-module-lexer in the list at the end of
  [doc/api/esm.md](../../api/esm.md)
  to point to the updated version.

* Create a PR, adding the files in the deps/cjs-module-lexer that
  were modified.

If updates are needed to cjs-module-lexer for Node.js, first PR
those updates into
[nodejs/cjs-module-lexer][],
request a release and then pull in the updated version once available.

[nodejs/cjs-module-lexer]: https://github.com/nodejs/cjs-module-lexer
