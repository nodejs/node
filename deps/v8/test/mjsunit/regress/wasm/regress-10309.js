// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

let registry = {};

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  let validated;
  try {
    validated = WebAssembly.validate(buffer);
  } catch (e) {
    throw new Error("Wasm validate throws");
  }
  if (validated !== valid) {
    throw new Error("Wasm validate failure" + (valid ? "" : " expected"));
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = registry) {
  return new WebAssembly.Instance(module(bytes), imports);
}

function call(instance, name, args) {
  return instance.exports[name](...args);
}

function exports(name, instance) {
  return {[name]: instance.exports};
}

function assert_return(action, expected) {
  let actual = action();
  if (!Object.is(actual, expected)) {
    throw new Error("Wasm return value " + expected + " expected, got " + actual);
  };
}

let f32 = Math.fround;

// simple.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x09\x02\x60\x00\x00\x60\x01\x7f\x01\x7d\x03\x04\x03\x00\x00\x01\x05\x03\x01\x00\x01\x07\x1c\x02\x11\x72\x65\x70\x6c\x61\x63\x65\x5f\x6c\x61\x6e\x65\x5f\x74\x65\x73\x74\x00\x01\x04\x72\x65\x61\x64\x00\x02\x08\x01\x00\x0a\x6e\x03\x2a\x00\x41\x10\x43\x00\x00\x80\x3f\x38\x02\x00\x41\x14\x43\x00\x00\x00\x40\x38\x02\x00\x41\x18\x43\x00\x00\x40\x40\x38\x02\x00\x41\x1c\x43\x00\x00\x80\x40\x38\x02\x00\x0b\x39\x01\x01\x7b\x41\x10\x2a\x02\x00\xfd\x13\x21\x00\x20\x00\x41\x10\x2a\x01\x04\xfd\x20\x01\x21\x00\x20\x00\x41\x10\x2a\x01\x08\xfd\x20\x02\x21\x00\x20\x00\x41\x10\x2a\x01\x0c\xfd\x20\x03\x21\x00\x41\x00\x20\x00\xfd\x0b\x02\x00\x0b\x07\x00\x20\x00\x2a\x02\x00\x0b");

// simple.wast:49
call($1, "replace_lane_test", []);

// simple.wast:50
assert_return(() => call($1, "read", [0]), f32(1.0));

// simple.wast:51
assert_return(() => call($1, "read", [4]), f32(2.0));

// simple.wast:52
assert_return(() => call($1, "read", [8]), f32(3.0));

// simple.wast:53
assert_return(() => call($1, "read", [12]), f32(4.0));
