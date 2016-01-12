//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
  var obj0 = {};
  var obj1 = {};
  var protoObj1 = {};
  var arrObj0 = {};
  var litObj1 = {};
  var func0 = function () {
  };
  var func1 = function () {
    return +null instanceof func0;
  };
  var func2 = function () {
  };
  var func4 = function () {
    func1();
  };
  obj0.method1 = func2;
  obj1.method0 = func4;
  Object.prototype.method1 = obj0.method1;
  var f32 = new Float32Array();
  var uic8 = new Uint8ClampedArray();
  var IntArr0 = [];
  var d = -522312766;
  var e = -3112571546786760000;
  var g = 142;
  if (!(typeof (func1.call(arrObj0, protoObj1.method1(arrObj0.prop0, uic8[obj0.prop0 & 255]), ++d, -0 instanceof (typeof Object == 'function' ? Object : Object)) == (e ? -1798973992 : e), IntArr0.push(65537 === arrObj0.prop1, obj0.prop1 * -8282417482912000000, g >>>= 142, parseInt('11002331320030312323212000200000', 4), obj0.prop0 ? 82 : obj1.prop2, IntArr0[(obj1.prop0 >= 0 ? obj1.prop0 : 0) & 15], typeof obj0.prop1 != null, 5954312838721990000, obj0.prop1 * -8282417482912000000, test0.caller, f32[Object.prototype.prop2 & 255], typeof obj0.prop1 != null, 3339474413495350000 * 65535 - d), test0.caller) == 'boolean')) {
    function func9() {
      var uniqobj8 = [obj1];
      uniqobj8[0].method0();
    }
    if (!func1(+((obj1.prop2 * (obj0.prop2 - obj1.length) instanceof (typeof Function == 'function' ? Function : Object)) >> func9.call(litObj1, /(?=(?=[a7]\W\b\d[郳7]))/g)))) {
    }
  }
}
test0();

function test1() {
  var func12 = function () {
  };
  if (new func12().prop0) {
    do {
      litObj1.prop0 = +-191;
      !litObj1.prop0.prop0;
    } while (test1);
  }
}
test1();
test1();
test1();

function test2() {
  Object.prototype['fireEvent'] = function () {
    return this;
  };
  print(window = 4277);
  for (var nllywf = 0; nllywf < 10; ++nllywf) {
    if (!(window = window.fireEvent())) {
    }
  }
}
test2();
test2();
test2();
test2();
test2();
test2();
test2();
