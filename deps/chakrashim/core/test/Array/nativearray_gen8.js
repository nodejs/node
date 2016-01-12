//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0(){
  var obj0 = {};
  var obj1 = {};
  var func0 = function(argArr0,argArr1,argFunc2){
    // Snippet : Array check hoist bailout if object is used instead of arrays.


    function v890195(v890196){
      for (var v890197 = 0 ; v890197 < 3 ; v890197++)
      {
        v890196[v890197] = (c |= argArr0[(((e >= 0 ? e : 0)) & 0XF)]);
        obj1.length = ary[((shouldBailout ? (ary[1] = "x") : undefined ), 1)];
      }
    }
    v890195(argArr1);
  }
  var func1 = function(argArr4,argFunc5){
    func0.call(obj0 , ary, ary, 1);
  }
  var ary = new Array(10);
  var c = 1;
  var e = 34;
  ary[0] = 1;
  ary[1] = 1;
  ary[2] = 1;
  ary[3] = 1;
  ary[4] = 1;
  ary[5] = 1;
  ary[6] = 1;
  ary[7] = 1;
  ary[8] = 1;
  ary[9] = 1;
  ary[10] = -3.60428436642705E+18;
  func1(1, 1);
};

// generate profile
test0();

// run JITted code
runningJITtedCode = true;
test0();

WScript.Echo('pass');
