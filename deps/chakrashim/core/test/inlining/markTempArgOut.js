//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//reduced switches: -maxsimplejitruncount:2 -maxinterpretcount:1

var arr=[];
var obj0 = {};
var func0 = function () {
};
obj0.method0 = func0;
var f32 = new Float32Array(256);
protoObj0 = Object(obj0);
for (var _strvar30 in f32) {
  function v9() {
    var v13 = {
      v14: function () {
        return function bar() {
          protoObj0.method0.apply(protoObj0, arguments);
          this.method0.apply(this.method0.apply(this, arguments), arguments);
        };
      }
    };
    protoObj0.v16 = v13.v14();
    protoObj0.v16.prototype = {
      method0: function (v20) {
        this.v20 = v20;
      }
    };

    new protoObj0.v16(f32[11]);
  }
  v9();
}
WScript.Echo("PASSED");
