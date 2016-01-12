//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

// Bug 1150770
function Processing() {
    var p = {};
    p.MathEval = function MathEval(str) {
          eval(str); // Comment this out to make the program work
    };  
    p.Fib = function Fib(n) {
                           if (n == 0) return 0;
                           if (n == 1) return 1;
                           return Fib(n-1) + Fib(n-2);  
    };
             return p;
  };

var p = Processing();
write(p.Fib(20));

// fusejs scenario
(function () {
    var first = function () { },
    second = function second() {
        var x = 1;
        var y = first();
        x = second;
        z = function () { return x; };
    };
    write("second test");
})();
