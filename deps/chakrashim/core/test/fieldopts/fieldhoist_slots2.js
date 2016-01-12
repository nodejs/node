//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var workItem = { increment: 1, isDone: false };
  var func0 = function() {
    workItem = { increment: 2, isDone: true }
  }
  while (!workItem.isDone) {
    for (var i = 0; i < 3 && !workItem.isDone; i += workItem.increment) {
      func0(i);
    }
  }
  WScript.Echo("i = " + i);
};

// generate profile
test0();

// run JITted code
test0();
