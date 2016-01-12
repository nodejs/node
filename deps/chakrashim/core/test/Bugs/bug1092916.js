//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
  var func3 = function (argMath3) {
    argMath3 = {};
    argMath3.prop0 = argMath3;
    argMath3.prop0;
  };
  func3();
}
test0();
test0();
test0();
test0();
WScript.Echo("PASSED");
