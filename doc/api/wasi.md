# WebAssembly System Interface (WASI)

<!--introduced_in=v13.3.0-->

> Stability: 1 - Experimental

The WASI API provides an implementation of the [WebAssembly System Interface][]
specification. WASI gives sandboxed WebAssembly applications access to the
underlying operating system via a collection of POSIX-like functions.

```js
'use strict';
const fs = require('fs');
const { WASI } = require('wasi');
const wasi = new WASI({
  args: process.argv,
  env: process.env,
  preopens: {
    '/sandbox': '/some/real/path/that/wasm/can/access'
  }
});
const importObject = { wasi_snapshot_preview1: wasi.wasiImport };

(async () => {
  const wasm = await WebAssembly.compile(fs.readFileSync('./binary.wasm'));
  const instance = await WebAssembly.instantiate(wasm, importObject);

  wasi.start(instance);
})();
```

The `--experimental-wasi-unstable-preview1` and `--experimental-wasm-bigint`
CLI arguments are needed for the previous example to run.

## Class: `WASI`
<!-- YAML
added: v13.3.0
-->

The `WASI` class provides the WASI system call API and additional convenience
methods for working with WASI-based applications. Each `WASI` instance
represents a distinct sandbox environment. For security purposes, each `WASI`
instance must have its command line arguments, environment variables, and
sandbox directory structure configured explicitly.

### `new WASI([options])`
<!-- YAML
added: v13.3.0
-->

* `options` {Object}
  * `args` {Array} An array of strings that the WebAssembly application will
    see as command line arguments. The first argument is the virtual path to the
    WASI command itself. **Default:** `[]`.
  * `env` {Object} An object similar to `process.env` that the WebAssembly
    application will see as its environment. **Default:** `{}`.
  * `preopens` {Object} This object represents the WebAssembly application's
    sandbox directory structure. The string keys of `preopens` are treated as
    directories within the sandbox. The corresponding values in `preopens` are
    the real paths to those directories on the host machine.
  * `returnOnExit` {boolean} By default, WASI applications terminate the Node.js
    process via the `__wasi_proc_exit()` function. Setting this option to `true`
    causes `wasi.start()` to return the exit code rather than terminate the
    process. **Default:** `false`.

### `wasi.start(instance)`
<!-- YAML
added: v13.3.0
-->

* `instance` {WebAssembly.Instance}

Attempt to begin execution of `instance` by invoking its `_start()` export.
If `instance` does not contain a `_start()` export, then `start()` attempts to
invoke the `__wasi_unstable_reactor_start()` export. If neither of those exports
is present on `instance`, then `start()` does nothing.

`start()` requires that `instance` exports a [`WebAssembly.Memory`][] named
`memory`. If `instance` does not have a `memory` export an exception is thrown.

### `wasi.wasiImport`
<!-- YAML
added: v13.3.0
-->

* {Object}

`wasiImport` is an object that implements the WASI system call API. This object
should be passed as the `wasi_snapshot_preview1` import during the instantiation
of a [`WebAssembly.Instance`][].

[`WebAssembly.Instance`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Instance
[`WebAssembly.Memory`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory
[WebAssembly System Interface]: https://wasi.dev/
