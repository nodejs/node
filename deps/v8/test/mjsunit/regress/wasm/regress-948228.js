// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var {proxy, revoke} = Proxy.revocable({}, {});
revoke();
let builder = new WasmModuleBuilder();
builder.addImport('m', 'q', kSig_v_v);
WebAssembly.instantiate(builder.toModule(), proxy);
