//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function(){
  var obj3 = 1;
  (function(){
    var obj7 = obj3;
    var obj8 = obj7;
    var __loopvar3 = 0;
    while ((obj7.b) && __loopvar3 < 3) {
      __loopvar3++;
      var a = ((obj7.b = 2147483647) <= obj8.b);
    }
  })();
})();
