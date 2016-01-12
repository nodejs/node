//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -forcenative -prejit 
(function(){
  var f = 1;
  f +=1;
  var __loopvar1 = 0;
  LABEL0: 
  LABEL1: 
  while((1) && __loopvar1 < 3) {
    __loopvar1++;
    a = (f &= 1);
    var __loopvar3 = 0;
    LABEL2: 
    LABEL3: 
    do {
      __loopvar3++;
      b >>>=(- f);
      break ;
    } while((1) && __loopvar3 < 3)
  }
})();
