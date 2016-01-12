//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var GiantPrintArray = [];
    var obj1 = {};
    function v9870() {
    try {
        var arr = [];
        var v9872 = [];
        Object.defineProperty(Array.prototype, "0", { configurable: true, get: function () { return 30; } });
        GiantPrintArray.push(v9872.indexOf(30));
      }
    catch(ex) {
        WScript.Echo(ex.message);
      }
    }
    v9870();
    WScript.Echo(GiantPrintArray[0]);
};
test0();
test0();

function test1() {
    try {
        var arr = [];
        Object.preventExtensions(arr);
        arr.push(0);
    }
    catch(ex) {
        WScript.Echo(ex.message);
    }
}
test1();
test1();

    Object.defineProperty(Object.prototype,"a",{get:function(){return 8 }});
function test2() {
var GiantPrintArray = [];
  var obj1 = {};
  var func1 = function(){
    try {
      GiantPrintArray.push(obj1.a);
    }
    catch(ex) {
        WScript.Echo(ex.message);
    }
  }
  func1();

  function v31079()
  {
    Object.defineProperty(Array.prototype, "4", {configurable : true, get: function(){return 15;}});
    try {
      GiantPrintArray.push(1);
      GiantPrintArray.push(1);
    }
    catch(ex) {
        WScript.Echo(ex.message);
    }

  }

  v31079();
  v31079();

  for(var i =0;i<GiantPrintArray.length;i++){
 WScript.Echo(GiantPrintArray[i]);
 };
}
test2();
test2();

