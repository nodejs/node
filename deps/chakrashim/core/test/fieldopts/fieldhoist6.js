//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -force:fieldhoist -prejit
(function(){
  var __loopvar2 = 0;
  LABEL0:
  LABEL1:
  do {
    __loopvar2++;
    f <<= ((a = (1 ? 1 : obj1.b)) + 1);
    var __loopvar3 = 0;
    do {
      __loopvar3++;
      a = (a++ );
    } while ((1) && __loopvar3 < 3)
  } while ((1) && __loopvar2 < 3)
})();
