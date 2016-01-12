//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var __counter = 0;
function test0(){
  __counter++;;
  var obj0 = {};
  var obj1 = {};
  var func1 = function(){
  }
  var func4 = function(){
  }
  obj0.method1 = func1;
  obj1.method1 = func4;
  protoObj0 = Object.create(obj0);
  protoObj1 = Object.create(obj1);
  // loopbreakblock5.ecs

  function v464() {
    var v465 = 0;
    var __loopvar1000 = 0;
  while(1) {
    if (__loopvar1000 > 3) break;
    __loopvar1000++;

        if(v465++ > 2) {
            var uniqobj0 = [protoObj1, protoObj0];
     uniqobj0[__counter%uniqobj0.length].method1();
            throw "loopbreakblock5.ecs";
        }
    }
  }

  try {
    v464();
  } catch(e) {};
};

// generate profile
test0();
// Run Simple JIT
test0();

// run JITted code
runningJITtedCode = true;
test0();
WScript.Echo("PASSED\n");
