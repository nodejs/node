//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test16(){
  var obj0 = {};
  var obj1 = {};
  var func2 = function(){
  }
  obj1.method0 = func2;
  var __loopvar1 = 0;
  do {
    __loopvar1++;
    obj1.method0.apply(obj0, arguments);
    obj1.method0;
  } while((1) && __loopvar1 < 3)
};

test16();
test16();

WScript.Echo("PASSED\n");
