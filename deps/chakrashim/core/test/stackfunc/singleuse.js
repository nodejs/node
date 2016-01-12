//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var litObj0;
var litObj1;
function test0(){
  var obj1 = {};
  //Snippet 2: more arguments than formal parameters
  obj1.prop0 = (function(x) {
    var fPolyProp = function (o) {
      if (o!==undefined) {
        WScript.Echo(o.prop0 + ' ' + o.prop1 + ' ' + o.prop2);
      }
    }
    fPolyProp(litObj0);
    fPolyProp(litObj1);

    return 1 + x;
  })(1.1,1,1.1);

};

// generate profile
test0();

// run JITted code
runningJITtedCode = true;
test0();

