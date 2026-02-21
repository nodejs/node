// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is not a test: it's infrastructure for building other tests,
// like wasm-module-builder.js.

function StringToArray(str) {
  let result = wasmUnsignedLeb(str.length);
  for (let c of str) result.push(c.charCodeAt(0));
  return result;
}

const kWasmMethod = 0;
const kWasmGetter = 1;
const kWasmSetter = 2;

// Assembles the data needed for one prototype. Don't construct this
// directly, use WasmPrototypeSetupBuilder.addConfig().
class WasmOnePrototypeConfigBuilder {
  constructor(prototype, parent) {
    this.prototype = prototype;
    this.parent = parent;
    this.constructor = undefined;
    this.statics = [];
    this.methods = [];
  }

  // name: string
  // func: a WasmFunctionBuilder for a function that returns an object of
  //       appropriate type.
  addConstructor(name, func) {
    if (this.constructor !== undefined) {
      throw new Error("Constructor already defined");
    }
    if (!(func instanceof WasmFunctionBuilder)) {
      throw new Error("{func} should be a WasmFunctionBuilder instance");
    }
    this.constructor = {name, func};
    return this;
  }

  // name: string
  // kind: one of kWasmMethod, kWasmGetter, kWasmSetter
  // func: a WasmFunctionBuilder
  addStatic(name, kind, func) {
    if (kind !== kWasmMethod && kind != kWasmGetter && kind != kWasmSetter) {
      throw new Error(
          "{kind} should be one of kWasmMethod, kWasmGetter, kWasmSetter");
    }
    if (!(func instanceof WasmFunctionBuilder)) {
      throw new Error("{func} should be a WasmFunctionBuilder instance");
    }
    this.statics.push({name, kind, func});
    return this;
  }

  // name: string
  // kind: one of kWasmMethod, kWasmGetter, kWasmSetter
  // func: a WasmFunctionBuilder
  addMethod(name, kind, func) {
    if (kind !== kWasmMethod && kind != kWasmGetter && kind != kWasmSetter) {
      throw new Error(
          "{kind} should be one of kWasmMethod, kWasmGetter, kWasmSetter");
    }
    if (!(func instanceof WasmFunctionBuilder)) {
      throw new Error("{func} should be a WasmFunctionBuilder instance");
    }
    this.methods.push({name, kind, func});
    return this;
  }
}

// Assembles a module's entire prototype configuration setup, consisting
// of many WasmOnePrototypeConfigBuilders.
class WasmPrototypeSetupBuilder {
  constructor(wasm_module_builder) {
    if (wasm_module_builder.functions.length !== 0) {
      throw new Error("Please initialize the WasmPrototypeSetupBuilder " +
                      "before defining functions!");
    }
    this.module_builder = wasm_module_builder;
    this.configs = [];
    this.$array_externref =
        wasm_module_builder.addArray(kWasmExternRef, true, kNoSuperType, true);
    this.$array_funcref =
        wasm_module_builder.addArray(kWasmFuncRef, true, kNoSuperType, true);
    this.$array_i8 =
        wasm_module_builder.addArray(kWasmI8, true, kNoSuperType, true);
    this.$configureAll = wasm_module_builder.addImport(
        "wasm:js-prototypes", "configureAll",
        makeSig([wasmRefNullType(this.$array_externref),
                wasmRefNullType(this.$array_funcref),
                wasmRefNullType(this.$array_i8), kWasmExternRef], []));
    this.$constructors = wasm_module_builder.addImportedGlobal(
        "c", "constructors", kWasmExternRef, false);
  }

  // prototype: index of the imported global holding the prototype.
  // parent: a previously created config.
  addConfig(prototype, parent = undefined) {
    if (typeof prototype != "number" ||
        (prototype >>> 0) >= this.module_builder.num_imported_globals) {
      throw new Error("{prototype} must be the index of an imported global");
    }
    let parent_index = undefined;
    if (parent === undefined) {
      parent_index = -1;
    } else {
      // Micro-optimization: scan backwards, for a better chance of
      // terminating early.
      for (let i = this.configs.length; i --> 0;) {
        if (this.configs[i] === parent) {
          parent_index = i;
          break;
        }
      }
      if (parent_index === undefined) {
        throw new Error("parent should be another prototype config");
      }
    }

    let config = new WasmOnePrototypeConfigBuilder(prototype, parent_index);
    this.configs.push(config);
    return config;
  }

  build() {
    let prototypes = [];
    let functions = [];
    let data = wasmUnsignedLeb(this.configs.length);

    for (let config of this.configs) {
      prototypes.push([kExprGlobalGet, config.prototype]);
      if (config.constructor) {
        data.push(1);
        data.push(...StringToArray(config.constructor.name));
        functions.push(config.constructor.func.index);
      } else if (config.statics.length > 0) {
        throw new Error("Can't have statics without having a constructor");
      }
      data.push(...wasmUnsignedLeb(config.statics.length));
      for (let stat of config.statics) {
        data.push(stat.kind);
        data.push(...StringToArray(stat.name));
        functions.push(stat.func.index);
      }
      data.push(...wasmUnsignedLeb(config.methods.length));
      for (let method of config.methods) {
        data.push(method.kind);
        data.push(...StringToArray(method.name));
        functions.push(method.func.index);
      }
      data.push(...wasmSignedLeb(config.parent));
    }

    let proto_segment = this.module_builder.addPassiveElementSegment(
        prototypes, kWasmExternRef);
    let func_segment = this.module_builder.addPassiveElementSegment(functions);
    let data_segment = this.module_builder.addPassiveDataSegment(data);

    let startfunc = this.module_builder.addFunction("start", kSig_v_v).addBody([
      kExprI32Const, 0,
      kExprI32Const, ...wasmSignedLeb(prototypes.length),
      kGCPrefix, kExprArrayNewElem, this.$array_externref, proto_segment,

      kExprI32Const, 0,
      kExprI32Const, ...wasmSignedLeb(functions.length),
      kGCPrefix, kExprArrayNewElem, this.$array_funcref, func_segment,

      kExprI32Const, 0,
      kExprI32Const, ...wasmSignedLeb(data.length),
      kGCPrefix, kExprArrayNewData, this.$array_i8, data_segment,
      kExprGlobalGet, this.$constructors,
      kExprCallFunction, this.$configureAll,
    ]);
    if (this.module_builder.start_index !== undefined) {
      throw new Error("Module builder already has a start function");
    }
    this.module_builder.addStart(startfunc.index);
  }
}
