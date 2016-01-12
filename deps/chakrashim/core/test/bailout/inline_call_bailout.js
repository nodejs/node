//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Configuration: inline.xml
//Testcase Number: 20382
//Switches:  -maxinterpretcount:4  -forceserialized -bgjit- -loopinterpretcount:1 -off:lossyinttypespec -off:arraycheckhoist  -version:5
//Baseline Switches: -nonative  -version:5
//Branch:  fbl_ie_script
//Build: 130517-2000
//Arch: AMD64
//MachineName: BPT02339
//InstructionSet: SSE2
function test0(){
  var func2 = function() {}
  function bar1 (argMath12,argMath13){
    WScript.Echo(argMath12);
  }
  function bar3 (argMath16,argMath17){
    bar1.call(null , argMath16, (((argMath16++ ) instanceof func2)) * func2.call(null));
  }
  bar3(false);
};

// generate profile
test0(); 
test0(); 
