//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var arrObj0 = {};
  var func2 = function(){
  }
  arrObj0.method0 = func2;
  var i32 = new Int32Array(2);
  var VarArr0 = 1;
  var __loopvar0 = 0;
  for(var _strvar27 in i32 ) {
    if(_strvar27.indexOf('method') != -1) continue;
    if(__loopvar0++ > 3) break;
    arrObj0.method0.call(Object.prototype , (arrObj0.length ? 351445441 : -496151049),  VarArr0[1]);
  }
};

test0();
test0();
test0();
WScript.Echo("PASSED\n");

function test1(){
  var obj0 = {};
  var func1 = function(){
  }
  obj0.method0 = func1;
  var ary = new Array(10);
  function _array2iterate(_array2tmp) {
    for(var _array2i in _array2tmp) {
      if(_array2tmp[_array2i] instanceof Array) {
        _array2iterate(_array2tmp[_array2i]);
      }
      obj0.method0.call(obj0 , ary[1]);
    }
  }
  _array2iterate([[obj0.prop1, [obj0.prop0]]]);
};

test1();
test1();
test1();
WScript.Echo("PASSED\n");

function test2(){
  function bar0 (){
    this.prop0;
  }

  var obj = {func: bar0};
  var __loopvar1 = 0;
  do {
    __loopvar1++;
        function v5524()
        {
            if(false)
            {
                var uniqobj0 = 1;
            }
            else
                return obj.func.call(uniqobj0);
        }

        v5524();
  } while((1) && __loopvar1 < 3)
};

test2();
test2();
test2();
WScript.Echo("PASSED\n");
