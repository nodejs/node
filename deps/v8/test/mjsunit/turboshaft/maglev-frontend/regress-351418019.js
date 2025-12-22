// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --turbolev --jit-fuzzing

this.WScript = new Proxy({}, {
  get() {
    switch (name) {
      case 'Echo':
    }
  }
});


var V8OptimizationStatus = {
  kIsFunction: 1 << 0,
};

function __f_3() {
  return [[]];
}

/* VariableOrObjectMutator: Random mutation */
try {

  __v_0, {
    get: function () {

      if (__v_1 != null && typeof __v_1 == "object") try {
        Object.defineProperty(__v_1, __v_0, 148676, {
        });
      } catch (e) {}

    }
  };

} catch (e) {}

try {
  __v_0 = __f_3();
  __v_1 = __f_3();
  ;
} catch (e) {}

function __f_4(__v_2, __v_3) {
  var __v_6 = { deopt: 0 };

  try {

    Object.defineProperty(__v_6, "deopt", {
      get: function () {
        d8.dom.EventTarget(__v_6[1], undefined);
        return undefined;
      }
    });

  } catch (e) {}
  try {
    __v_4 = __v_3(__v_2,
                  30, __v_6);
    ;
  } catch (e) {}

  try {
    if (__v_0 != null && typeof __v_0 == "object") Object.defineProperty(__v_0, "deopt", {
      value: d8.debugger
    });
  } catch (e) {}
}


function __f_6(__v_13, __v_14, __v_15, __v_16) {
  new __v_13(__v_14, __v_15, __v_16);
}

function __f_8(__v_21) {
  try {
    __f_4();
  } catch (e) {}

  __f_4(__v_21, __f_6);

}

function __f_9(__v_22, __v_23) {
  try {
    __v_23.deopt;
  } catch (e) {}
}

try {
  __f_8(__f_9);
} catch (e) {}
