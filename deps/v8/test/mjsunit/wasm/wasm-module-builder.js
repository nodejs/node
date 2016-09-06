// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  emit_varint(val) {
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
      this.emit_varint(string.length);
      this.emit_bytes(string);
      return;
    }

    // This is the hacky way to convert a JavaScript string to a UTF8 encoded
    // string only containing single-byte characters.
    let string_utf8 = unescape(encodeURIComponent(string));
    this.emit_varint(string_utf8.length);
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
    this.emit_string(section_names[section_code]);
    // Emit the section to a temporary buffer: its full length isn't know yet.
    let section = new Binary;
    content_generator(section);
    // Emit section length.
    this.emit_varint(section.length);
    // Copy the temporary buffer.
    this.push(...section);
  }
}

class WasmFunctionBuilder {
  constructor(name, type_index) {
    this.name = name;
    this.type_index = type_index;
    this.exports = [];
  }

  exportAs(name) {
    this.exports.push(name);
    return this;
  }

  exportFunc() {
    this.exports.push(this.name);
    return this;
  }

  addBody(body) {
    this.body = body;
    return this;
  }

  addLocals(locals) {
    this.locals = locals;
    return this;
  }
}

class WasmModuleBuilder {
  constructor() {
    this.types = [];
    this.imports = [];
    this.functions = [];
    this.exports = [];
    this.table = [];
    this.segments = [];
    this.explicit = [];
    this.pad = null;
    return this;
  }

  addStart(start_index) {
    this.start_index = start_index;
  }

  addMemory(min, max, exp) {
    this.memory = {min: min, max: max, exp: exp};
    return this;
  }

  addPadFunctionTable(size) {
    this.pad = size;
    return this;
  }

  addExplicitSection(bytes) {
    this.explicit.push(bytes);
    return this;
  }

  addType(type) {
    // TODO: canonicalize types?
    this.types.push(type);
    return this.types.length - 1;
  }

  addFunction(name, type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    let func = new WasmFunctionBuilder(name, type_index);
    func.index = this.functions.length;
    this.functions.push(func);
    return func;
  }

  addImportWithModule(module, name, type) {
    let type_index = (typeof type) == "number" ? type : this.addType(type);
    this.imports.push({module: module, name: name, type: type_index});
    return this.imports.length - 1;
  }

  addImport(name, type) {
    return this.addImportWithModule(name, undefined, type);
  }

  addDataSegment(addr, data, init) {
    this.segments.push({addr: addr, data: data, init: init});
    return this.segments.length - 1;
  }

  appendToTable(array) {
    this.table.push(...array);
    return this;
  }

  toArray(debug) {
    let binary = new Binary;
    let wasm = this;

    // Add header
    binary.emit_header();

    // Add type section
    if (wasm.types.length > 0) {
      if (debug) print("emitting types @ " + binary.length);
      binary.emit_section(kDeclTypes, section => {
        section.emit_varint(wasm.types.length);
        for (let type of wasm.types) {
          section.emit_u8(kWasmFunctionTypeForm);
          section.emit_varint(type.params.length);
          for (let param of type.params) {
            section.emit_u8(param);
          }
          section.emit_varint(type.results.length);
          for (let result of type.results) {
            section.emit_u8(result);
          }
        }
      });
    }

    // Add imports section
    if (wasm.imports.length > 0) {
      if (debug) print("emitting imports @ " + binary.length);
      binary.emit_section(kDeclImports, section => {
        section.emit_varint(wasm.imports.length);
        for (let imp of wasm.imports) {
          section.emit_varint(imp.type);
          section.emit_string(imp.module);
          section.emit_string(imp.name || '');
        }
      });
    }

    // Add functions declarations
    let has_names = false;
    let names = false;
    let exports = 0;
    if (wasm.functions.length > 0) {
      if (debug) print("emitting function decls @ " + binary.length);
      binary.emit_section(kDeclFunctions, section => {
        section.emit_varint(wasm.functions.length);
        for (let func of wasm.functions) {
          has_names = has_names || (func.name != undefined &&
                                   func.name.length > 0);
          exports += func.exports.length;
          section.emit_varint(func.type_index);
        }
      });
    }

    // Add table.
    if (wasm.table.length > 0) {
      if (debug) print("emitting table @ " + binary.length);
      binary.emit_section(kDeclTable, section => {
        section.emit_varint(wasm.table.length);
        if (wasm.pad !== null) {
          if (debug) print("emitting table padding @ " + binary.length);
          section.emit_varint(wasm.pad);
        }
        for (let index of wasm.table) {
          section.emit_varint(index);
        }
      });
    }

    // Add memory section
    if (wasm.memory != undefined) {
      if (debug) print("emitting memory @ " + binary.length);
      binary.emit_section(kDeclMemory, section => {
        section.emit_varint(wasm.memory.min);
        section.emit_varint(wasm.memory.max);
        section.emit_u8(wasm.memory.exp ? 1 : 0);
      });
    }


    // Add export table.
    if (exports > 0) {
      if (debug) print("emitting exports @ " + binary.length);
      binary.emit_section(kDeclExports, section => {
        section.emit_varint(exports);
        for (let func of wasm.functions) {
          for (let exp of func.exports) {
            section.emit_varint(func.index);
            section.emit_string(exp);
          }
        }
      });
    }

    // Add start function section.
    if (wasm.start_index != undefined) {
      if (debug) print("emitting start function @ " + binary.length);
      binary.emit_section(kDeclStart, section => {
        section.emit_varint(wasm.start_index);
      });
    }

    // Add function bodies.
    if (wasm.functions.length > 0) {
      // emit function bodies
      if (debug) print("emitting code @ " + binary.length);
      binary.emit_section(kDeclCode, section => {
        section.emit_varint(wasm.functions.length);
        for (let func of wasm.functions) {
          // Function body length will be patched later.
          let local_decls = [];
          let l = func.locals;
          if (l != undefined) {
            let local_decls_count = 0;
            if (l.i32_count > 0) {
              local_decls.push({count: l.i32_count, type: kAstI32});
            }
            if (l.i64_count > 0) {
              local_decls.push({count: l.i64_count, type: kAstI64});
            }
            if (l.f32_count > 0) {
              local_decls.push({count: l.f32_count, type: kAstF32});
            }
            if (l.f64_count > 0) {
              local_decls.push({count: l.f64_count, type: kAstF64});
            }
          }

          let header = new Binary;
          header.emit_varint(local_decls.length);
          for (let decl of local_decls) {
            header.emit_varint(decl.count);
            header.emit_u8(decl.type);
          }

          section.emit_varint(header.length + func.body.length);
          section.emit_bytes(header);
          section.emit_bytes(func.body);
        }
      });
    }

    // Add data segments.
    if (wasm.segments.length > 0) {
      if (debug) print("emitting data segments @ " + binary.length);
      binary.emit_section(kDeclData, section => {
        section.emit_varint(wasm.segments.length);
        for (let seg of wasm.segments) {
          section.emit_varint(seg.addr);
          section.emit_varint(seg.data.length);
          section.emit_bytes(seg.data);
        }
      });
    }

    // Add any explicitly added sections
    for (let exp of wasm.explicit) {
      if (debug) print("emitting explicit @ " + binary.length);
      binary.emit_bytes(exp);
    }

    // Add function names.
    if (has_names) {
      if (debug) print("emitting names @ " + binary.length);
      binary.emit_section(kDeclNames, section => {
        section.emit_varint(wasm.functions.length);
        for (let func of wasm.functions) {
          var name = func.name == undefined ? "" : func.name;
          section.emit_string(name);
          section.emit_u8(0);  // local names count == 0
        }
      });
    }

    return binary;
  }

  toBuffer(debug) {
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

  instantiate(...args) {
    let module = new WebAssembly.Module(this.toBuffer());
    let instance = new WebAssembly.Instance(module, ...args);
    return instance;
  }
}
