//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function(){
  var obj0 = 1;
  obj0.b = obj0.b & 1;
  WScript.Echo("obj0.b = " + (obj0.b|0));;
})();

function f(x){return x};
function foo() {
    var obj = {}
    obj.x = 5;
    obj = f(obj.x);
    WScript.Echo(obj.x);
}
foo()
