//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var str = new String('testing contains');
var p = new Proxy(str, {});
// Object.defineProperty(p, "toString", {value : function(arg) { print('proxys toString'); return "b"; }});
Object.defineProperty(p, "valueOf", {value : function(arg) { print('proxys valueOf'); return "c"; }});
print(p + "a");

var n = new Number(100);
var p1 = new Proxy(n, {});
Object.defineProperty(p1, "valueOf", {value : function(arg) { print('proxys valueOf'); return 10; }});
print(p1 + 5);

try{
    var p2 = new Proxy(new Number(5), {});
    p2 + 5;
} catch (e) {
        if (!(e instanceof TypeError) || e.message !== "Number.prototype.valueOf: 'this' is not a Number object") {
            $ERROR(e);
        }
 }
 print('PASS');
