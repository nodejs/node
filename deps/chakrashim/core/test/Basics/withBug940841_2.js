//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test4() {
  with ({ x: 1 % {}}) 
  {
    for (var i = 0; i < 1; i++) {
      x;
    }
  }
}
test4();
test4();
WScript.Echo("PASS");
