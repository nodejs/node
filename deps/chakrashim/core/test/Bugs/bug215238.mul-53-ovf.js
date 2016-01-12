//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var obj0 = {};
  var arrObj0 = {};
  var func0 = function(){
    l -=((-733600173 * arrObj0.prop0) & (-731419186 * (-733600173 * arrObj0.prop0)));
  };
  var func1 = function(argArr0){
    k = func0.call(arrObj0 );
  };
  obj0.method0 = func1;
  var l = 1;
  arrObj0.prop0 = -38;
  m = obj0.method0.call(obj0 , 1);
  WScript.Echo('l = ' + (l|0));
};

// generate profile
test0();
// Run Simple JIT
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();
