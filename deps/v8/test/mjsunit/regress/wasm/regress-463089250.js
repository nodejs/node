// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
WebAssembly.Module.prototype.then = resolve => resolve(17);

assertPromiseResult(
    WebAssembly.instantiate(builder.toBuffer()), assertUnreachable,
    e => assertException(
        e, TypeError,
        'Illegal call to WebAssembly.instantiate callback: ' +
            'Argument is not a WasmModuleObject'));
