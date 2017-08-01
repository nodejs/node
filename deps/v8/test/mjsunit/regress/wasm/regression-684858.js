// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

var name = 'regression_684858';

function patchNameLength(buffer) {
  var count = 0;
  var view = new Uint8Array(buffer);
  for (var i = 0, e = view.length - name.length; i <= e; ++i) {
    var subs = String.fromCharCode.apply(null, view.slice(i, i + name.length));
    if (subs != name) continue;
    ++count;
    // One byte before this name, its length is encoded.
    // Patch this to 127, making it out of bounds.
    if (view.length >= 127) throw Error('cannot patch reliably');
    if (view[i - 1] != name.length) throw Error('unexpected length');
    view[i - 1] = 0x7f;
  }
  if (count != 1) throw Error('did not find name');
}

var builder = new WasmModuleBuilder();
builder.addFunction(name, kSig_i_v)
    .addBody([kExprI32Const, 2, kExprI32Const, 0, kExprI32DivU])
    .exportAs('main');
var buffer = builder.toBuffer();
patchNameLength(buffer);
var module = new WebAssembly.Module(buffer);
var instance = new WebAssembly.Instance(module);
assertThrows(() => instance.exports.main(), WebAssembly.RuntimeError);
