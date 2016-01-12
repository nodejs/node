//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0(){
  var obj0 = {};
  var obj1 = {};
  var obj2 = {};
  var func0 = function(p0){
  }
  var func1 = function(){
    if(ary[(((((shouldBailout ? (ary[1] = "x") : undefined ), 1) >= 0 ? 1 : 0)) & 0XF)]) {
      var __loopvar3 = 0;
      while((func0(1, (shouldBailout ? (Object.defineProperty(obj0, 'prop1', {get: function() { WScript.Echo('obj0.prop1 getter'); return 3; }}), 1) : 1), 1, 1)) && __loopvar3 < 3) {
        __loopvar3++;
      }
    }
  }
  var func2 = function(){
    if(((ary[(((obj1.prop3 >= 0 ? obj1.prop3 : 0)) & 0XF)] ? func1(1, 1, 1, 1, 1) : 1) + func1())) {
    }
  }
  Object.prototype.method0 = func2;
  var ary = new Array(10);
  this.prop2 = {prop0: 1, prop1: 1, prop2: 1, prop3: 1, prop4: 1, prop5: 1, prop6: 1, prop7: (shouldBailout ? (Object.defineProperty(this, 'prop2', {set: function(_x) { WScript.Echo('this.prop2 setter'); }}), 1) : 1)};
  this.prop5 = {prop7: 1, prop6: (-- obj2.prop6), prop5: 1, prop4: ary[(((((shouldBailout ? (ary[Math.acos(1)] = "x") : undefined ), Math.acos(1)) >= 0 ? Math.acos(1) : 0)) & 0XF)], prop3: obj1.method0(1), prop2: 1, prop1: 1, prop0: 1};
};

// generate profile
test0();

// run JITted code
test0();

// run code with bailouts enabled
shouldBailout = true;
test0();
