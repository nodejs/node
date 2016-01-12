//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var i = 0;
while (i < 3) 
{
  (function () 
  {
    with ({}) 
    {
      __proto__;
    }
  })();
  i++;
}
WScript.Echo("PASS");
