// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const typedArrayIntConstructors = [
  {name: "Uint8", ctor: Uint8Array},
  {name: "Int8", ctor: Int8Array},
  {name: "Uint16", ctor: Uint16Array},
  {name: "Int16", ctor: Int16Array},
  {name: "Uint32", ctor: Uint32Array},
  {name: "Int32", ctor: Int32Array},
  {name: "Uint8Clamped", ctor: Uint8ClampedArray},
];

const typedArrayFloatConstructors = [
  {name: "Float32", ctor: Float32Array},
  {name: "Float64", ctor: Float64Array},
];

// "ref" builds might not yet have BigInt support, so the benchmark fails
// gracefully during setup (the constructor will be undefined), instead of
// a hard fail when this file is loaded.
const typedArrayBigIntConstructors = [
  {name: "BigUint64", ctor: this["BigUint64Array"]},
  {name: "BigInt64", ctor: this["BigInt64Array"]}
];
