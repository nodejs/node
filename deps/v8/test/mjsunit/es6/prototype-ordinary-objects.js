// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var funcs = [

  // https://code.google.com/p/v8/issues/detail?id=4002
  // Error,
  // EvalError,
  // RangeError,
  // ReferenceError,
  // SyntaxError,
  // TypeError,
  // URIError,

  // https://code.google.com/p/v8/issues/detail?id=4003
  // RegExp,

  // https://code.google.com/p/v8/issues/detail?id=4004
  // Date,

  // https://code.google.com/p/v8/issues/detail?id=4006
  // String,

  // Boolean,
  // Number,
  // https://code.google.com/p/v8/issues/detail?id=4001

  ArrayBuffer,
  DataView,
  Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  Map,
  Object,
  Promise,
  // Proxy,
  Set,
  Symbol,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
  WeakMap,
  WeakSet,
];

for (var fun of funcs) {
  var p = fun.prototype;

  // @@toStringTag is tested separately, and interferes with this test.
  if (Symbol.toStringTag) delete p[Symbol.toStringTag];
  assertEquals('[object Object]', Object.prototype.toString.call(p));
}


// These still have special prototypes for legacy reason.
var funcs = [
  Array,
  Function,
];

for (var fun of funcs) {
  var p = fun.prototype;
  assertEquals(`[object ${fun.name}]`, Object.prototype.toString.call(p));
}
