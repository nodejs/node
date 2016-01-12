//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//reduced switches: -mic:1 -forcejitloopbody -off:interpreterautoprofile

function SmallObjectConstr() {
  this.prop0 = 0;
}

var obj2 = { prop3: {} };

(function () {

  var obj = new SmallObjectConstr();
  obj2.prop3 += obj.prop2++;
  obj.prop2++;
  
  for (var i of [1,2]) {
    obj.prop3 = { a: obj.prop2++ };
    obj.prop2 = 4;
  }
})();

WScript.Echo("PASSED");