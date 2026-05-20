'use strict';

// This file stores JS builtins that can not become primordials yet because
// they can be disabled by runtime flags, or are not enabled by default yet.

// Without this module, the only choice we would have is getting them from
// the global proxy which is mutable from userland. This is especially important
// for lazy-loaded builtins required in modules participating in early bootstrap.
// In such modules, we are usually unable to get these builtins synchronously,
// this applies to synchronous destructuring of this module.
// Importing the module itself is fine at any point, including top level of file.

let _AsyncDisposableStack;
let _DisposableStack;
let _Float16Array;
let _Intl;
let _Iterator;
let _ShadowRealm;
let _SharedArrayBuffer;
let _Temporal;
let _Uint8ArrayFromHex;
let _Uint8ArrayFromBase64;
let _WebAssembly;

module.exports = {
  get AsyncDisposableStack() {
    return _AsyncDisposableStack;
  },
  get DisposableStack() {
    return _DisposableStack;
  },
  get Float16Array() {
    return _Float16Array;
  },
  get Intl() {
    return _Intl;
  },
  get Iterator() {
    return _Iterator;
  },
  get ShadowRealm() {
    return _ShadowRealm;
  },
  get SharedArrayBuffer() {
    return _SharedArrayBuffer;
  },
  get Temporal() {
    return _Temporal;
  },
  get Uint8ArrayFromHex() {
    return _Uint8ArrayFromHex;
  },
  get Uint8ArrayFromBase64() {
    return _Uint8ArrayFromBase64;
  },
  get WebAssembly() {
    return _WebAssembly;
  },
  _init({
    AsyncDisposableStack,
    DisposableStack,
    Float16Array,
    Intl,
    Iterator,
    ShadowRealm,
    SharedArrayBuffer,
    Temporal,
    Uint8Array: {
      fromBase64,
      fromHex,
    },
    WebAssembly,
  }) {
    _AsyncDisposableStack = AsyncDisposableStack;
    _DisposableStack = DisposableStack;
    _Float16Array = Float16Array;
    _Intl = Intl;
    _Iterator = Iterator;
    _ShadowRealm = ShadowRealm;
    _SharedArrayBuffer = SharedArrayBuffer;
    _Temporal = Temporal;
    _Uint8ArrayFromBase64 = fromBase64;
    _Uint8ArrayFromHex = fromHex;
    _WebAssembly = WebAssembly;

    delete this._init;
  },
};
