// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for encoding f32 and double constants to bits.
let __buffer = new ArrayBuffer(8);
let byte_view = new Int8Array(__buffer);
let f32_view = new Float32Array(__buffer);
let f64_view = new Float64Array(__buffer);

class Binary extends Array {
  emit_u8(val) {
    this.push(val);
  }

  emit_u16(val) {
    this.push(val & 0xff);
    this.push((val >> 8) & 0xff);
  }

  emit_u32(val) {
    this.push(val & 0xff);
    this.push((val >> 8) & 0xff);
    this.push((val >> 16) & 0xff);
    this.push((val >> 24) & 0xff);
  }

  emit_u32v(val) {
    while (true) {
      let v = val & 0xff;
      val = val >>> 7;
      if (val == 0) {
        this.push(v);
        break;
      }
      this.push(v | 0x80);
    }
  }

  emit_bytes(data) {
    for (let i = 0; i < data.length; i++) {
      this.push(data[i] & 0xff);
    }
  }

  emit_string(string) {
    // When testing illegal names, we pass a byte array directly.
    if (string instanceof Array) {
      this.emit_u32v(string.length);
      this.emit_bytes(string);
      return;
    }

    // This is the hacky way to convert a JavaScript string to a UTF8 encoded
    // string only containing single-byte characters.
    let string_utf8 = unescape(encodeURIComponent(string));
    this.emit_u32v(string_utf8.length);
    for (let i = 0; i < string_utf8.length; i++) {
      this.emit_u8(string_utf8.charCodeAt(i));
    }
  }

  emit_header() {
    this.push(kWasmH0, kWasmH1, kWasmH2, kWasmH3,
              kWasmV0, kWasmV1, kWasmV2, kWasmV3);
  }

  emit_section(section_code, content_generator) {
    // Emit section name.
    this.emit_u8(section_code);
    // Emit the section to a temporary buffer: its full length isn't know yet.
    let section = new Binary;
    content_generator(section);
    // Emit section length.
    this.emit_u32v(section.length);
    // Copy the temporary buffer.
    for (var i = 0; i < section.length; i++) this.push(section[i]);
  }
}

class WasmFunctionBuilder {
  constructor(module, name, type_index) {
    this.module = module;
    this.name = name;
    this.type_index = type_index;
    this.body = [];
  }

  numLocalNames() {
    if (this.local_names === undefined) return 0;
    let num_local_names = 0;
    for (let loc_name of this.local_names) {
      if (loc_name !== undefined) ++num_local_names;
    }
    return num_local_names;
  }

  exportAs(name) {
    this.module.addExport(name, this.index);
    return this;
  }

  exportFunc() {
    this.exportAs(this.name);
    return this;
  }

  addBody(body) {
    for (let b of body) {
      if (typeof b !== 'number' || (b & (~0xFF)) !== 0 )
        throw new Error('invalid body (entries must be 8 bit numbers): ' + body);
    }
    this.body = body.slice();
    // Automatically add the end for the function block to the body.
    this.body.push(kExprEnd);
    return this;
  }

  addBodyWithEnd(body) {
    this.body = body;
    return this;
  }

  getNumLocals() {
    let total_locals = 0;
    for (let l of this.locals || []) {
      for (let type of ["i32", "i64", "f32", "f64", "s128"]) {
        total_locals += l[type + "_count"] || 0;
      }
    }
    return total_locals;
  }

  addLocals(locals, names) {
    const old_num_locals = this.getNumLocals();
    if (!this.locals) this.locals = []
    this.locals.push(locals);
    if (names) {
      if (!this.local_names) this.local_names = [];
      const missing_names = old_num_locals - this.local_names.length;
      this.local_names.push(...new Array(missing_names), ...names);
    }
    return this;
  }

  end() {
    return this.module;
  }
}

class WasmGlobalBuilder {
  constructor(module, type, mutable) {
    this.module = module;
    this.type = type;
    this.mutable = mutable;
    this.init = 0;
  }

  exportAs(name) {
    this.module.exports.push({name: name, kind: kExternalGlobal,
                              index: this.index});
    return this;
  }
}

class WasmModuleBuilder {
  constructor() {
    this.types = [];
    this.imports = [];
    this.exports = [];
    this.globals = [];
    this.exceptions = [];
    this.functions = [];
    this.function_table = [];
    this.function_table_length_min = 0;
    this.function_table_length_max = 0;
    this.function_table_inits = [];
    this.segments = [];
    this.explicit = [];
    this.num_imported_funcs = 0;
    this.num_imported_globals = 0;
    return this;
  }

  addStart(start_index) {
    this.start_index = start_index;
    return this;
  }

  addMemory(min, max, exp, shared) {
    this.memory = {min: min, max: max, exp: exp, shared: shared};
    return this;
  }

  addExplicitSection(bytes) {
    this.explicit.push(bytes);
    return this;
  }

  stringToBytes(name) {
    var result = new Binary();
    result.emit_u32v(name.length);
    for (var i = 0; i < name.length; i++) {
      result.emit_u8(name.charCodeAt(i));
    }
    return result;
  }

  addCustomSection(name, bytes) {
    name = this.stringToBytes(name);
    var length = new Binary();
    length.emit_u32v(name.length + bytes.length);
    this.explicit.push([0, ...length, ...name, ...bytes]);
  }

  addType(type) {
    this.types.push(type);
    var pl = type.params.length;  // should have params
    var rl = type.results.length; // should have results
    return this.types.length - 1;
  }

  addGlobal(local_type, mutable) {
    let glob = new WasmGlobalBuilder(this, local_type, mutable);
    glob.index = this.globals.length + this.num_imported_globals;
    this.globals.push(glob);
    return glob;
  }

  addException(type) {
    if (type.results.length != 0)
      throw new Error('Invalid exception signature: ' + type);
    this.exceptions.push(type);
    return this.exceptions.length - 1;
  }

  addFunction(name, type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let func = new WasmFunctionBuilder(this, name, type_index);
    func.index = this.functions.length + this.num_imported_funcs;
    this.functions.push(func);
    return func;
  }

  addImport(module = "", name, type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    this.imports.push({module: module, name: name, kind: kExternalFunction,
                       type: type_index});
    return this.num_imported_funcs++;
  }

  addImportedGlobal(module = "", name, type) {
    let o = {module: module, name: name, kind: kExternalGlobal, type: type,
             mutable: false}
    this.imports.push(o);
    return this.num_imported_globals++;
  }

  addImportedMemory(module = "", name, initial = 0, maximum, shared) {
    let o = {module: module, name: name, kind: kExternalMemory,
             initial: initial, maximum: maximum, shared: shared};
    this.imports.push(o);
    return this;
  }

  addImportedTable(module = "", name, initial, maximum) {
    let o = {module: module, name: name, kind: kExternalTable, initial: initial,
             maximum: maximum};
    this.imports.push(o);
  }

  addExport(name, index) {
    this.exports.push({name: name, kind: kExternalFunction, index: index});
    return this;
  }

  addExportOfKind(name, kind, index) {
    this.exports.push({name: name, kind: kind, index: index});
    return this;
  }

  addDataSegment(addr, data, is_global = false) {
    this.segments.push({addr: addr, data: data, is_global: is_global});
    return this.segments.length - 1;
  }

  exportMemoryAs(name) {
    this.exports.push({name: name, kind: kExternalMemory, index: 0});
  }

  addFunctionTableInit(base, is_global, array, is_import = false) {
    this.function_table_inits.push({base: base, is_global: is_global,
                                    array: array});
    if (!is_global) {
      var length = base + array.length;
      if (length > this.function_table_length_min && !is_import) {
        this.function_table_length_min = length;
      }
      if (length > this.function_table_length_max && !is_import) {
         this.function_table_length_max = length;
      }
    }
    return this;
  }

  appendToTable(array) {
    for (let n of array) {
      if (typeof n != 'number')
        throw new Error('invalid table (entries have to be numbers): ' + array);
    }
    return this.addFunctionTableInit(this.function_table.length, false, array);
  }

  setFunctionTableBounds(min, max) {
    this.function_table_length_min = min;
    this.function_table_length_max = max;
    return this;
  }

  setName(name) {
    this.name = name;
    return this;
  }

  toArray(debug = false) {
    let binary = new Binary;
    let wasm = this;

    // Add header
    binary.emit_header();

    // Add type section
    if (wasm.types.length > 0) {
      if (debug) print("emitting types @ " + binary.length);
      binary.emit_section(kTypeSectionCode, section => {
        section.emit_u32v(wasm.types.length);
        for (let type of wasm.types) {
          section.emit_u8(kWasmFunctionTypeForm);
          section.emit_u32v(type.params.length);
          for (let param of type.params) {
            section.emit_u8(param);
          }
          section.emit_u32v(type.results.length);
          for (let result of type.results) {
            section.emit_u8(result);
          }
        }
      });
    }

    // Add imports section
    if (wasm.imports.length > 0) {
      if (debug) print("emitting imports @ " + binary.length);
      binary.emit_section(kImportSectionCode, section => {
        section.emit_u32v(wasm.imports.length);
        for (let imp of wasm.imports) {
          section.emit_string(imp.module);
          section.emit_string(imp.name || '');
          section.emit_u8(imp.kind);
          if (imp.kind == kExternalFunction) {
            section.emit_u32v(imp.type);
          } else if (imp.kind == kExternalGlobal) {
            section.emit_u32v(imp.type);
            section.emit_u8(imp.mutable);
          } else if (imp.kind == kExternalMemory) {
            var has_max = (typeof imp.maximum) != "undefined";
            var is_shared = (typeof imp.shared) != "undefined";
            if (is_shared) {
              section.emit_u8(has_max ? 3 : 2); // flags
            } else {
              section.emit_u8(has_max ? 1 : 0); // flags
            }
            section.emit_u32v(imp.initial); // initial
            if (has_max) section.emit_u32v(imp.maximum); // maximum
          } else if (imp.kind == kExternalTable) {
            section.emit_u8(kWasmAnyFunctionTypeForm);
            var has_max = (typeof imp.maximum) != "undefined";
            section.emit_u8(has_max ? 1 : 0); // flags
            section.emit_u32v(imp.initial); // initial
            if (has_max) section.emit_u32v(imp.maximum); // maximum
          } else {
            throw new Error("unknown/unsupported import kind " + imp.kind);
          }
        }
      });
    }

    // Add functions declarations
    if (wasm.functions.length > 0) {
      if (debug) print("emitting function decls @ " + binary.length);
      binary.emit_section(kFunctionSectionCode, section => {
        section.emit_u32v(wasm.functions.length);
        for (let func of wasm.functions) {
          section.emit_u32v(func.type_index);
        }
      });
    }

    // Add function_table.
    if (wasm.function_table_length_min > 0) {
      if (debug) print("emitting table @ " + binary.length);
      binary.emit_section(kTableSectionCode, section => {
        section.emit_u8(1);  // one table entry
        section.emit_u8(kWasmAnyFunctionTypeForm);
        // TODO(gdeepti): Cleanup to use optional max flag,
        // fix up tests to set correct initial/maximum values
        section.emit_u32v(1);
        section.emit_u32v(wasm.function_table_length_min);
        section.emit_u32v(wasm.function_table_length_max);
      });
    }

    // Add memory section
    if (wasm.memory !== undefined) {
      if (debug) print("emitting memory @ " + binary.length);
      binary.emit_section(kMemorySectionCode, section => {
        section.emit_u8(1);  // one memory entry
        const has_max = wasm.memory.max !== undefined;
        const is_shared = wasm.memory.shared !== undefined;
        // Emit flags (bit 0: reszeable max, bit 1: shared memory)
        if (is_shared) {
          section.emit_u8(has_max ? 3 : 2);
        } else {
          section.emit_u8(has_max ? 1 : 0);
        }
        section.emit_u32v(wasm.memory.min);
        if (has_max) section.emit_u32v(wasm.memory.max);
      });
    }

    // Add global section.
    if (wasm.globals.length > 0) {
      if (debug) print ("emitting globals @ " + binary.length);
      binary.emit_section(kGlobalSectionCode, section => {
        section.emit_u32v(wasm.globals.length);
        for (let global of wasm.globals) {
          section.emit_u8(global.type);
          section.emit_u8(global.mutable);
          if ((typeof global.init_index) == "undefined") {
            // Emit a constant initializer.
            switch (global.type) {
            case kWasmI32:
              section.emit_u8(kExprI32Const);
              section.emit_u32v(global.init);
              break;
            case kWasmI64:
              section.emit_u8(kExprI64Const);
              section.emit_u32v(global.init);
              break;
            case kWasmF32:
              section.emit_u8(kExprF32Const);
              f32_view[0] = global.init;
              section.emit_u8(byte_view[0]);
              section.emit_u8(byte_view[1]);
              section.emit_u8(byte_view[2]);
              section.emit_u8(byte_view[3]);
              break;
            case kWasmF64:
              section.emit_u8(kExprF64Const);
              f64_view[0] = global.init;
              section.emit_u8(byte_view[0]);
              section.emit_u8(byte_view[1]);
              section.emit_u8(byte_view[2]);
              section.emit_u8(byte_view[3]);
              section.emit_u8(byte_view[4]);
              section.emit_u8(byte_view[5]);
              section.emit_u8(byte_view[6]);
              section.emit_u8(byte_view[7]);
              break;
            }
          } else {
            // Emit a global-index initializer.
            section.emit_u8(kExprGetGlobal);
            section.emit_u32v(global.init_index);
          }
          section.emit_u8(kExprEnd);  // end of init expression
        }
      });
    }

    // Add export table.
    var mem_export = (wasm.memory !== undefined && wasm.memory.exp);
    var exports_count = wasm.exports.length + (mem_export ? 1 : 0);
    if (exports_count > 0) {
      if (debug) print("emitting exports @ " + binary.length);
      binary.emit_section(kExportSectionCode, section => {
        section.emit_u32v(exports_count);
        for (let exp of wasm.exports) {
          section.emit_string(exp.name);
          section.emit_u8(exp.kind);
          section.emit_u32v(exp.index);
        }
        if (mem_export) {
          section.emit_string("memory");
          section.emit_u8(kExternalMemory);
          section.emit_u8(0);
        }
      });
    }

    // Add start function section.
    if (wasm.start_index !== undefined) {
      if (debug) print("emitting start function @ " + binary.length);
      binary.emit_section(kStartSectionCode, section => {
        section.emit_u32v(wasm.start_index);
      });
    }

    // Add table elements.
    if (wasm.function_table_inits.length > 0) {
      if (debug) print("emitting table @ " + binary.length);
      binary.emit_section(kElementSectionCode, section => {
        var inits = wasm.function_table_inits;
        section.emit_u32v(inits.length);

        for (let init of inits) {
          section.emit_u8(0); // table index
          if (init.is_global) {
            section.emit_u8(kExprGetGlobal);
          } else {
            section.emit_u8(kExprI32Const);
          }
          section.emit_u32v(init.base);
          section.emit_u8(kExprEnd);
          section.emit_u32v(init.array.length);
          for (let index of init.array) {
            section.emit_u32v(index);
          }
        }
      });
    }

    // Add exceptions.
    if (wasm.exceptions.length > 0) {
      if (debug) print("emitting exceptions @ " + binary.length);
      binary.emit_section(kExceptionSectionCode, section => {
        section.emit_u32v(wasm.exceptions.length);
        for (let type of wasm.exceptions) {
          section.emit_u32v(type.params.length);
          for (let param of type.params) {
            section.emit_u8(param);
          }
        }
      });
    }

    // Add function bodies.
    if (wasm.functions.length > 0) {
      // emit function bodies
      if (debug) print("emitting code @ " + binary.length);
      binary.emit_section(kCodeSectionCode, section => {
        section.emit_u32v(wasm.functions.length);
        for (let func of wasm.functions) {
          // Function body length will be patched later.
          let local_decls = [];
          for (let l of func.locals || []) {
            if (l.i32_count > 0) {
              local_decls.push({count: l.i32_count, type: kWasmI32});
            }
            if (l.i64_count > 0) {
              local_decls.push({count: l.i64_count, type: kWasmI64});
            }
            if (l.f32_count > 0) {
              local_decls.push({count: l.f32_count, type: kWasmF32});
            }
            if (l.f64_count > 0) {
              local_decls.push({count: l.f64_count, type: kWasmF64});
            }
            if (l.s128_count > 0) {
              local_decls.push({count: l.s128_count, type: kWasmS128});
            }
          }

          let header = new Binary;
          header.emit_u32v(local_decls.length);
          for (let decl of local_decls) {
            header.emit_u32v(decl.count);
            header.emit_u8(decl.type);
          }

          section.emit_u32v(header.length + func.body.length);
          section.emit_bytes(header);
          section.emit_bytes(func.body);
        }
      });
    }

    // Add data segments.
    if (wasm.segments.length > 0) {
      if (debug) print("emitting data segments @ " + binary.length);
      binary.emit_section(kDataSectionCode, section => {
        section.emit_u32v(wasm.segments.length);
        for (let seg of wasm.segments) {
          section.emit_u8(0);  // linear memory index 0
          if (seg.is_global) {
            // initializer is a global variable
            section.emit_u8(kExprGetGlobal);
            section.emit_u32v(seg.addr);
          } else {
            // initializer is a constant
            section.emit_u8(kExprI32Const);
            section.emit_u32v(seg.addr);
          }
          section.emit_u8(kExprEnd);
          section.emit_u32v(seg.data.length);
          section.emit_bytes(seg.data);
        }
      });
    }

    // Add any explicitly added sections
    for (let exp of wasm.explicit) {
      if (debug) print("emitting explicit @ " + binary.length);
      binary.emit_bytes(exp);
    }

    // Add names.
    let num_function_names = 0;
    let num_functions_with_local_names = 0;
    for (let func of wasm.functions) {
      if (func.name !== undefined) ++num_function_names;
      if (func.numLocalNames() > 0) ++num_functions_with_local_names;
    }
    if (num_function_names > 0 || num_functions_with_local_names > 0 ||
        wasm.name !== undefined) {
      if (debug) print('emitting names @ ' + binary.length);
      binary.emit_section(kUnknownSectionCode, section => {
        section.emit_string('name');
        // Emit module name.
        if (wasm.name !== undefined) {
          section.emit_section(kModuleNameCode, name_section => {
            name_section.emit_string(wasm.name);
          });
        }
        // Emit function names.
        if (num_function_names > 0) {
          section.emit_section(kFunctionNamesCode, name_section => {
            name_section.emit_u32v(num_function_names);
            for (let func of wasm.functions) {
              if (func.name === undefined) continue;
              name_section.emit_u32v(func.index);
              name_section.emit_string(func.name);
            }
          });
        }
        // Emit local names.
        if (num_functions_with_local_names > 0) {
          section.emit_section(kLocalNamesCode, name_section => {
            name_section.emit_u32v(num_functions_with_local_names);
            for (let func of wasm.functions) {
              if (func.numLocalNames() == 0) continue;
              name_section.emit_u32v(func.index);
              name_section.emit_u32v(func.numLocalNames());
              for (let i = 0; i < func.local_names.length; ++i) {
                if (func.local_names[i] === undefined) continue;
                name_section.emit_u32v(i);
                name_section.emit_string(func.local_names[i]);
              }
            }
          });
        }
      });
    }

    return binary;
  }

  toBuffer(debug = false) {
    let bytes = this.toArray(debug);
    let buffer = new ArrayBuffer(bytes.length);
    let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; i++) {
      let val = bytes[i];
      if ((typeof val) == "string") val = val.charCodeAt(0);
      view[i] = val | 0;
    }
    return buffer;
  }

  instantiate(ffi) {
    let module = new WebAssembly.Module(this.toBuffer());
    let instance = new WebAssembly.Instance(module, ffi);
    return instance;
  }

  toModule(debug = false) {
    return new WebAssembly.Module(this.toBuffer(debug));
  }
}
