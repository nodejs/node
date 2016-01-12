//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function(){
  var func0 = function(){
    d = { valueOf: function() { WScript.Echo('d valueOf'); return 3; } };
  }
  var func1 = function(p0){
    p0 = (p0 >>>= (~ func0(1)));
  }
  var d = 1;
  func1(d);
  func1(d);
})();
