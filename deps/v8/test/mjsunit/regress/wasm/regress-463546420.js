// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

assertThrows(() => {
    const v0 = /Qa\nb\bc/gvism;
    function f4(a5) {
        const v6 = [a5];
        const v7 = v6[0];
        let v8 = new WebAssembly.Tag({ parameters: ["externref", "anyref", "f32", "externref", "eqref", "i31ref", "v128"] });
        String.prototype.startsWith.call("NFD", "localeCompare", v7);
        [v0, ...v6, v7];
        return v8.propertyIsEnumerable(Symbol);
    }
    Symbol.toString = f4;
    const v16 = {
        [Symbol]() {
        },
    };
}, RangeError);
