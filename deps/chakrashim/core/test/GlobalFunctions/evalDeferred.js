//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var s1 =

'function f1() {' +
'    var a = "a";' +
'    function g1() {' +
'        WScript.Echo(a);' +
'    }' +
'    return g1;' +
'}';

eval(s1);
WScript.Echo('done s1');
var foo1 = f1();
WScript.Echo('done f1');
foo1();

var s2 =

'var a = "global a";' +
'function f2(i) {' +
'    with ({a:"with a"}) {' +
'        var g2 = function() {' +
'            WScript.Echo(a);' +
'        };' +
'        function g2_() {' +
'            WScript.Echo(a);' +
'        }' +
'    }' +
'    switch(i) {' +
'        case 0: return g2;' +
'        case 1: return g2_;' +
'    }' +
'}';

eval(s2);
WScript.Echo('done s2');
var foo2 = f2(0);
var foo2_ = f2(1);
WScript.Echo('done f2');
foo2();
foo2_();

var s3 = 

'function f3(i) {' +
'    var a = "f3 a";' +
'    function g3(i) {' +
'        try {' +
'            throw "catch";' +
'        }' +
'        catch(a) {' +
'            function g4_() {' +
'                WScript.Echo(a);' +
'            }' +
'            var g4 = function() {' +
'                WScript.Echo(a);' +
'            };' +
'            return i == 0 ? g4 : g4_;' +
'        }' +
'    }' +
'    return g3(i);' +
'}';

eval(s3);
WScript.Echo('done s3');
var foo3 = f3(0);
var foo3_ = f3(1);
WScript.Echo('done f3');
foo3();
foo3_();
