//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

  var obj3 = {b:1};
  (function(){
    var obj7 = obj3;
    var obj8 = obj7;
    var sum = 0;
    for (var i = 0; i < 2; i++)
    {
      sum += obj7.b;
      sum += obj8.b;
      obj8.b = 3;
      sum += obj7.b;
    }
    WScript.Echo(sum);
  })();
