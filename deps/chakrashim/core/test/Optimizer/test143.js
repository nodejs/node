//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
  var o = {};
  o[0] = 0;
  var o2 = o;
  for (var i = 0; i < 2; ++i) {
    o2[0];
    for (var j = 0; j < 1; ++j) {
      if (false) {
        ({ f: function() {} });
      }
      for (var k = 0; k < 1; ++k) {
        o2[0];
      }
      test0a();
    }
  }

  function test0a() { o2; };
}
test0();
