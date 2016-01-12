//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var glo;
var Proxy = function () {
};
var print = function (x) {
    glo = x;
};
(function () {
    Object = function kspuxw() {
        print(kspuxw);
    }(Proxy(function handlerFactory() {
        return {
            getOwnPropertyDescriptor: function () {
                var yum = 'PCAL';
                dumpln(yum + 'LED: getOwnPropertyDescriptor');
            }
        };
    }()));
}());

glo();

(function (argMath5) {
  function v0() {
    (function () {
      function v2() {
      }
      argMath5 = eval;
    })(arguments[0]);
    prop1 = argMath5;
  }
  v0();
})();
WScript.Echo(prop1);

WScript.Echo("PASS");
