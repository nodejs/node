// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example crashes if the AllocationSites have not been properly added to
// the allocation_sites_list.

// Flags: --expose-gc

function __getProperties() {
    return [];
    let properties = [];
    for (let name of Object.getOwnPropertyNames()) {;
    }
    return properties;
}

function __getRandomProperty() {
    let properties = __getProperties();
    if (!properties.length)
        return undefined;
    return properties[seed % properties.length];
}
var kWasmH0 = 0;
var kWasmH1 = 0x61;
var kWasmH2 = 0x73;
var kWasmH3 = 0x6d;
var kWasmV0 = 0x1;
var kWasmV1 = 0;
var kWasmV2 = 0;
var kWasmV3 = 0;
class Binary extends Array {
    emit_header() {
        this.push(kWasmH0, kWasmH1, kWasmH2, kWasmH3,
            kWasmV0, kWasmV1, kWasmV2, kWasmV3);
    }
}
class WasmModuleBuilder {
    constructor() {
        this.exports = [];
    }
    addImportedMemory() {}
    setFunctionTableLength() {}
    toArray() {
        let binary = new Binary;
        let wasm = this;
        binary.emit_header();
        "emitting imports @ " + binary.length;
        section => {};
        var mem_export = (wasm.memory !== undefined && wasm.memory.exp);
        var exports_count = wasm.exports.length + (mem_export ? 1 : 0);
        return binary;
    }
    toBuffer() {
        let bytes = this.toArray();
        let buffer = new ArrayBuffer(bytes.length);
        let view = new Uint8Array(buffer);
        for (let i = 0; i < bytes.length; i++) {
            let val = bytes[i];
            view[i] = val | 0;
        }
        return buffer;
    }
    instantiate(ffi) {
        let module = new WebAssembly.Module(this.toBuffer());
        let instance = new WebAssembly.Instance(module);
    }
}

var v_40 = 0;
var v_43 = NaN;

try {
    v_23 = new WasmModuleBuilder();
} catch (e) {
    print("Caught: " + e);
}

try {
    v_31 = [0xff];
    v_29 = [v_31];
} catch (e) {
    print("Caught: " + e);
}

try {
    v_25 = ["main"];
    gc();
} catch (e) {
    print("Caught: " + e);
}
for (var v_28 of [[2]]) {
    try {
        gc();
    } catch (e) {
        print("Caught: " + e);
    }
}
try {
    module = v_23.instantiate();
} catch (e) {
    print("Caught: " + e);
}
try {
    v_41 = [];
} catch (e) {;
}
for (var v_43 = 0; v_43 < 100000; v_43++) try {
    v_41[v_43] = [];
} catch (e) {
    "Caught: " + e;
}
