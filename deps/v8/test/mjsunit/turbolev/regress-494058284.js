// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags = --invocation-count-for-maglev=10 --invocation-count-for-turbofan=20

function __wrapTC(f, permissive = true) {
  return f();
}
var __v_5 = 'Testing identifiers with double-byte names';
var __v_16 = 'ʴ = Z';
function __f_5() {
  return {
    params: __v_5,
  };
}
class __c_1 {
  emit_bytes(__v_68) {
    Array().join();
    if (__v_68 != null && typeof __v_68 == "object") try {
    } catch (e) { }
  }
  emit_heap_type() {
    this.emit_bytes(__f_7());
  }
  emit_type() {
    try {
      if (typeof __v_72 == 'number') {
      } else {
        this.emit_heap_type();
      }
    } catch (e) { }
  }
  emit_section(__v_73, __v_74) {
    const __v_75 = __wrapTC(() => new __c_1());
    __v_74(__v_75);
  }
}
class __c_3 { }
class __c_4 {
}
class __c_5 {
  constructor() {
    this.types = [];
  }
  addType(__v_82 = __v_44 = __v_84 = false) {
    var __v_85 = {
      params: __v_82.params,
    };
    this.types.push(__v_85);
  }
  addFunction(__v_89, __v_90) {
    let __v_91 = __wrapTC(() => typeof __v_89 == 'number' ? __v_90 : this.addType(__v_90));
  }
  toBuffer() {
    let __v_95 = __wrapTC(() => new __c_1());
    let __v_96 = __wrapTC(() => this);
    __v_95.emit_section(__v_95, __v_98 => {
      for (let __v_100 = 0; __v_100 < __v_96.types.length; __v_100++) {
        let __v_101 = __v_96.types[__v_100];
        if (__v_101 instanceof __c_3) { } else if (__v_101 instanceof __c_4) {
        } else {
          for (let __v_102 of __v_101.params) {
            __v_98.emit_type();
          }
        }
      }
    });
  }
  instantiate() {
    let __v_110 = __wrapTC(() => this.toModule());
  }
  toModule() {
    this.toBuffer();
  }
}

function __f_7() {
  let __v_114 = __wrapTC(() => []);
  let __v_115 = __wrapTC(() => __v_16 & 0x7f);
  __v_114.__v_115;
  return __v_114;
}


let __v_52 = new __c_5();
__v_52.addType(__f_5());
__v_52.instantiate();
