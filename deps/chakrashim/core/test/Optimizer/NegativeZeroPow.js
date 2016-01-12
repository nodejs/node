//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var a = -2;
  var ui16 = new Uint16Array(1);
  for(var __loopvar0 = 0; __loopvar0 < 3 && 1 < Math.pow(- ui16[0], a-- ); __loopvar0++) {
    WScript.Echo("blah");
  }
};

// generate profile
test0();

// run JITted code
test0();


