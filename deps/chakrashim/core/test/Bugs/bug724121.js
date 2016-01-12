//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
  var GiantPrintArray = [];
  var v26 = {};
  Object.defineProperty(Object.prototype, '__getterprop4', {
    get: function () {
      function v0() {
      }
      v0.prototype.v2 = function () {
      };
      var v3 = new v0();
      function v4() {
      }
      v4.prototype.v2 = function () {
      };
      var v6 = new v4();
      function v17(v18) {
        v18.v2();
      }
      v17(v3);
      v17(v6);
    }, configurable:true
  });
  GiantPrintArray.push(v26.__getterprop4);
  for (;;) {
    break;

    for (var _strvar0 in IntArr0) {
    }
    GiantPrintArray.push(v30.__getterprop4);
  }
}
test0();
test0();
test0();
WScript.Echo("PASS");
