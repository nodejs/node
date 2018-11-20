// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  this.WScript = new Proxy({}, {
    get() {
      switch (name) {
      }
    }
  });
function MjsUnitAssertionError() {
};
let __v_692 = `(function module() { "use asm";function foo(`;
const __v_693 =
3695;
for (let __v_695 = 0; __v_695 < __v_693; ++__v_695) {
    __v_692 += `arg${__v_695},`;
}
try {
  __v_692 += `arg${__v_693}){`;
} catch (e) {}
for (let __v_696 = 0; __v_696 <= __v_693; ++__v_696) {
    __v_692 += `arg${__v_696}=+arg${__v_696};`;
}
  __v_692 += "return 10;}function bar(){return foo(";
for (let __v_697 = 0; __v_697 < __v_693; ++__v_697) {
    __v_692 += "0.0,";
}
  __v_692 += "1.0)|0;}";

  __v_692 += "return bar})()()";
const __v_694 = eval(__v_692);
