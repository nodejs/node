//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//reduced switches: -ForceArrayBTree
function test0() {
  var c = 4294967295;
  var ary = Array();
  var func2 = function () {
    ary.pop();
    ary.pop();
    return 4;
  };

  function func3() {
    --c
    ary.reverse();
    return func2()+ 1;
  }

  ary[c] = 1;
  ary.splice(0, 0, func2(), func3());
  ary.push(2);
  ary[c] = 0;
  ary.splice(1, 0, func2(), func3());
  ary.push(3);
}
test0();
print("PASS");