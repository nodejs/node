// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __isPropertyOfType(obj, name) {
    Object.getOwnPropertyDescriptor(obj, name)
}
function __getProperties(obj, type) {
  for (let name of Object.getOwnPropertyNames(obj)) {
    __isPropertyOfType(obj, name);
  }
}
function __getRandomProperty(obj) {
  let properties = __getProperties(obj);
}
function __f_6776(__v_33890, __v_33891) {
  var __v_33896 = __v_33891();
 __getRandomProperty([])
}
(function __f_6777() {
  var __v_33906 = async () => { };
    __f_6776(1, () => __v_33906())
})();
(function __f_6822() {
  try {
    __f_6776(1, () => __f_6822());
  } catch (e) {}
  var __v_34059 = async (__v_34079 = () => eval()) => { };
    delete __v_34059[__getRandomProperty(__v_34059)];
})();
