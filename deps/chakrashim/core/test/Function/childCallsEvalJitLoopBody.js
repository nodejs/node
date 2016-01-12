//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Due to the eval, 'a' get put in a slot and should not be assigned a non-temp register. The uses of 'a' in the loop should be
// given a temp register so that they are not loaded/restored from the jitted loop body.
(function(){
  var __loopvar0 = 0;
  while((1) && __loopvar0 < 3) {
    __loopvar0++;
    for(var __loopvar1 = 0; __loopvar1 < 3; ++__loopvar1) {
      (function(){
        (function(){
          eval("");
        })(1, 1, 1, 1);
        var __loopvar3 = 0;
        while((1) && __loopvar3 < 3) {
          __loopvar3++;
          d =Math.sin((-1012552393 * (__loopvar1 << a)));
          var a = 1;
        }
      })(1);
    }
  }
})();
