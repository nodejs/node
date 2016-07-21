// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function WasmFunctionBuilder(name, sig_index) {
    this.name = name;
    this.sig_index = sig_index;
    this.exports = [];
}

WasmFunctionBuilder.prototype.exportAs = function(name) {
    this.exports.push(name);
    return this;
}

WasmFunctionBuilder.prototype.exportFunc = function() {
  this.exports.push(this.name);
  return this;
}

WasmFunctionBuilder.prototype.addBody = function(body) {
    this.body = body;
    return this;
}

WasmFunctionBuilder.prototype.addLocals = function(locals) {
    this.locals = locals;
    return this;
}

function WasmModuleBuilder() {
    this.signatures = [];
    this.imports = [];
    this.functions = [];
    this.exports = [];
    this.function_table = [];
    this.data_segments = [];
    this.explicit = [];
    return this;
}

WasmModuleBuilder.prototype.addStart = function(start_index) {
    this.start_index = start_index;
}

WasmModuleBuilder.prototype.addMemory = function(min, max, exp) {
    this.memory = {min: min, max: max, exp: exp};
    return this;
}

WasmModuleBuilder.prototype.addExplicitSection = function(bytes) {
  this.explicit.push(bytes);
  return this;
}

// Add a signature; format is [param_count, param0, param1, ..., retcount, ret0]
WasmModuleBuilder.prototype.addSignature = function(sig) {
    // TODO: canonicalize signatures?
    this.signatures.push(sig);
    return this.signatures.length - 1;
}

WasmModuleBuilder.prototype.addFunction = function(name, sig) {
    var sig_index = (typeof sig) == "number" ? sig : this.addSignature(sig);
    var func = new WasmFunctionBuilder(name, sig_index);
    func.index = this.functions.length;
    this.functions.push(func);
    return func;
}

WasmModuleBuilder.prototype.addImportWithModule = function(module, name, sig) {
  var sig_index = (typeof sig) == "number" ? sig : this.addSignature(sig);
  this.imports.push({module: module, name: name, sig_index: sig_index});
  return this.imports.length - 1;
}

WasmModuleBuilder.prototype.addImport = function(name, sig) {
  this.addImportWithModule(name, undefined, sig);
}

WasmModuleBuilder.prototype.addDataSegment = function(addr, data, init) {
    this.data_segments.push({addr: addr, data: data, init: init});
    return this.data_segments.length - 1;
}

WasmModuleBuilder.prototype.appendToFunctionTable = function(array) {
    this.function_table = this.function_table.concat(array);
    return this;
}

function emit_u8(bytes, val) {
    bytes.push(val & 0xff);
}

function emit_u16(bytes, val) {
    bytes.push(val & 0xff);
    bytes.push((val >> 8) & 0xff);
}

function emit_u32(bytes, val) {
    bytes.push(val & 0xff);
    bytes.push((val >> 8) & 0xff);
    bytes.push((val >> 16) & 0xff);
    bytes.push((val >> 24) & 0xff);
}

function emit_string(bytes, string) {
    // When testing illegal names, we pass a byte array directly.
    if (string instanceof Array) {
      emit_varint(bytes, string.length);
      emit_bytes(bytes, string);
      return;
    }

    // This is the hacky way to convert a JavaScript scring to a UTF8 encoded
    // string only containing single-byte characters.
    var string_utf8 = unescape(encodeURIComponent(string));
    emit_varint(bytes, string_utf8.length);
    for (var i = 0; i < string_utf8.length; i++) {
      emit_u8(bytes, string_utf8.charCodeAt(i));
    }
}

function emit_varint(bytes, val) {
    while (true) {
        var v = val & 0xff;
        val = val >>> 7;
        if (val == 0) {
            bytes.push(v);
            break;
        }
        bytes.push(v | 0x80);
    }
}

function emit_bytes(bytes, data) {
  for (var i = 0; i < data.length; i++) {
    bytes.push(data[i] & 0xff);
  }
}

function emit_section(bytes, section_code, content_generator) {
    // Emit section name.
    emit_string(bytes, section_names[section_code]);
    // Emit the section to a temporary buffer: its full length isn't know yet.
    var tmp_bytes = [];
    content_generator(tmp_bytes);
    // Emit section length.
    emit_varint(bytes, tmp_bytes.length);
    // Copy the temporary buffer.
    Array.prototype.push.apply(bytes, tmp_bytes);
}

WasmModuleBuilder.prototype.toArray = function(debug) {
    // Add header bytes
    var bytes = [];
    bytes = bytes.concat([kWasmH0, kWasmH1, kWasmH2, kWasmH3,
                          kWasmV0, kWasmV1, kWasmV2, kWasmV3]);

    var wasm = this;

    // Add signatures section
    if (wasm.signatures.length > 0) {
        if (debug) print("emitting signatures @ " + bytes.length);
        emit_section(bytes, kDeclSignatures, function(bytes) {
            emit_varint(bytes, wasm.signatures.length);
            for (sig of wasm.signatures) {
                emit_u8(bytes, kWasmFunctionTypeForm);
                for (var j = 0; j < sig.length; j++) {
                    emit_u8(bytes, sig[j]);
                }
            }
        });
    }

    // Add imports section
    if (wasm.imports.length > 0) {
        if (debug) print("emitting imports @ " + bytes.length);
        emit_section(bytes, kDeclImportTable, function(bytes) {
            emit_varint(bytes, wasm.imports.length);
            for (imp of wasm.imports) {
                emit_varint(bytes, imp.sig_index);
                emit_string(bytes, imp.module);
                emit_string(bytes, imp.name || '');
            }
        });
    }

    // Add functions declarations
    var names = false;
    var exports = 0;
    if (wasm.functions.length > 0) {
        var has_names = false;

        // emit function signatures
        if (debug) print("emitting function sigs @ " + bytes.length);
        emit_section(bytes, kDeclFunctionSignatures, function(bytes) {
            emit_varint(bytes, wasm.functions.length);
            for (func of wasm.functions) {
              has_names = has_names || (func.name != undefined &&
                                        func.name.length > 0);
              exports += func.exports.length;

              emit_varint(bytes, func.sig_index);
            }
        });

    }

    // Add function table.
    if (wasm.function_table.length > 0) {
        if (debug) print("emitting function table @ " + bytes.length);
        emit_section(bytes, kDeclFunctionTable, function(bytes) {
            emit_varint(bytes, wasm.function_table.length);
            for (index of wasm.function_table) {
                emit_varint(bytes, index);
            }
        });
    }

    // Add memory section
    if (wasm.memory != undefined) {
        if (debug) print("emitting memory @ " + bytes.length);
        emit_section(bytes, kDeclMemory, function(bytes) {
            emit_varint(bytes, wasm.memory.min);
            emit_varint(bytes, wasm.memory.max);
            emit_u8(bytes, wasm.memory.exp ? 1 : 0);
        });
    }


    // Add export table.
    if (exports > 0) {
        if (debug) print("emitting exports @ " + bytes.length);
        emit_section(bytes, kDeclExportTable, function(bytes) {
            emit_varint(bytes, exports);
            for (func of wasm.functions) {
                for (exp of func.exports) {
                    emit_varint(bytes, func.index);
                    emit_string(bytes, exp);
                }
            }
        });
    }

    // Add start function section.
    if (wasm.start_index != undefined) {
        if (debug) print("emitting start function @ " + bytes.length);
        emit_section(bytes, kDeclStartFunction, function(bytes) {
            emit_varint(bytes, wasm.start_index);
        });
    }

    // Add function bodies.
    if (wasm.functions.length > 0) {
        // emit function bodies
        if (debug) print("emitting function bodies @ " + bytes.length);
        emit_section(bytes, kDeclFunctionBodies, function(bytes) {
            emit_varint(bytes, wasm.functions.length);
            for (func of wasm.functions) {
                // Function body length will be patched later.
                var local_decls = [];
                var l = func.locals;
                if (l != undefined) {
                  var local_decls_count = 0;
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
                var header = new Array();

                emit_varint(header, local_decls.length);
                for (decl of local_decls) {
                  emit_varint(header, decl.count);
                  emit_u8(header, decl.type);
                }

                emit_varint(bytes, header.length + func.body.length);
                emit_bytes(bytes, header);
                emit_bytes(bytes, func.body);
            }
        });
    }

    // Add data segments.
    if (wasm.data_segments.length > 0) {
        if (debug) print("emitting data segments @ " + bytes.length);
        emit_section(bytes, kDeclDataSegments, function(bytes) {
            emit_varint(bytes, wasm.data_segments.length);
            for (seg of wasm.data_segments) {
                emit_varint(bytes, seg.addr);
                emit_varint(bytes, seg.data.length);
                emit_bytes(bytes, seg.data);
            }
        });
    }

    // Add any explicitly added sections
    for (exp of wasm.explicit) {
        if (debug) print("emitting explicit @ " + bytes.length);
        emit_bytes(bytes, exp);
    }

    // Add function names.
    if (has_names) {
        if (debug) print("emitting names @ " + bytes.length);
        emit_section(bytes, kDeclNames, function(bytes) {
            emit_varint(bytes, wasm.functions.length);
            for (func of wasm.functions) {
                var name = func.name == undefined ? "" : func.name;
                emit_string(bytes, name);
                emit_u8(bytes, 0);  // local names count == 0
            }
        });
    }

    // End the module.
    if (debug) print("emitting end @ " + bytes.length);
    emit_section(bytes, kDeclEnd, function(bytes) {});

    return bytes;
}

WasmModuleBuilder.prototype.toBuffer = function(debug) {
    var bytes = this.toArray(debug);
    var buffer = new ArrayBuffer(bytes.length);
    var view = new Uint8Array(buffer);
    for (var i = 0; i < bytes.length; i++) {
        var val = bytes[i];
        if ((typeof val) == "string") val = val.charCodeAt(0);
        view[i] = val | 0;
    }
    return buffer;
}

WasmModuleBuilder.prototype.instantiate = function(ffi, memory) {
    var buffer = this.toBuffer();
    if (memory != undefined) {
      return Wasm.instantiateModule(buffer, ffi, memory);
    } else {
      return Wasm.instantiateModule(buffer, ffi);
    }
}
