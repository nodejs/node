//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var func4 = function () {
  function func5() { 
    if( func5.caller == null || func5.arguments == null)
    {
        print("FAILED")
    }
  }
  func5(func5());
};
func4();
print("PASSED");
