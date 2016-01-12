//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(x) {
    WScript.Echo("test apply simple call with one argument: " + x );
}
test.apply(null, ["val1", "val2", "val3"]);

function test1(x, y, z) {
    WScript.Echo("test apply simple call with  3 arguments: " + x + " , " + y + " , " + z);

}
test1.apply(null, ["p1", "p2", "p3"]);

function test2(x, y) {
    WScript.Echo("In test2 apply ");
    this.a = x;
    this.b = y
}
var o1 = new Object();
test2.apply(o1, [9, "secondValue"]);
WScript.Echo("test apply call to function that sets properties in 'this': " + o1.a + " , " + o1.b );

function test3() {
    WScript.Echo("In test3 apply ");
    this.a = "param1";
    this.b = 99
}

test3.apply();
WScript.Echo("test apply call to function that sets properties in global 'this': " + a + " , " + b);

function testArg(x, y, z) {
    WScript.Echo("**run tests with Arguments object");
    test.apply(null, arguments);

    test1.apply(null, arguments);

    var o1 = new Object();
    test2.apply(o1, arguments);
    WScript.Echo("test apply call to function that sets properties in 'this': " + o1.a + " , " + o1.b);
   
    
}

testArg("1stArg", "2ndArg", "3rdArg");