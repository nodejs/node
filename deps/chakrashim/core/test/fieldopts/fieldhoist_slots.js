//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function(){
  var obj0 = new Object();
  for (var __loopvar3 = 0; __loopvar3 < 3; ++__loopvar3) {
  }
  for (var __loopvar3 = 0; __loopvar3 < 3; ++__loopvar3) {
    obj0.a = 1;
  }
  function func8 (){
    eval("");
  }
  WScript.Echo("obj0.a = " + (obj0.a|0));
})();

WScript.Echo((function() {
    var e = 1;
    var func1 = function() {
        e = -1;
    }
    while (e-- > 0) {
        func1();
    }
    return e;
})());
