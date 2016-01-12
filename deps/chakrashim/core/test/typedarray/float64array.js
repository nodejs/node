//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("util.js");

function oneTest(a)
{
a[1] = -0.65;
a[5] = 10;
WScript.Echo(a[5]);
if (Object.getOwnPropertyDescriptor(a, 100000) != undefined) {
    WScript.Echo('FAIL');
}

try {
    var pro = Float64Array.prototype;
    WScript.Echo(pro.toString());

    WScript.Echo("prototype is");
    printObj(pro);
} catch(e) {
    WScript.Echo("constructor is");
    printObj(Float64Array);
}

WScript.Echo("object is");
printObj(a);

a[20] =20;
a.foo ='bar';
WScript.Echo("object after expando is");
printObj(a);
WScript.Echo("");
}

WScript.Echo("test1");
var test1 = new Float64Array(9);
oneTest(test1);

WScript.Echo("test2");
var test2 = new Float64Array(0);
oneTest(test2);

WScript.Echo("test3");
var arrayBuffer = new ArrayBuffer(32);
var test3 = new Float64Array(arrayBuffer);
oneTest(test3);

WScript.Echo("test4");
var test4 = new Float64Array(arrayBuffer, 8);
oneTest(test4);

WScript.Echo("test5");
var test5 = new Float64Array(arrayBuffer, 8, 3);
oneTest(test5);

WScript.Echo("test6");
var mybuffer = test1.buffer; 
WScript.Echo(mybuffer);
var test6 = new Float64Array(mybuffer);
oneTest(test6);

WScript.Echo("test7");
var test7 = new Float64Array(test1.buffer, 8);
oneTest(test7);

WScript.Echo("test8");
var test8 = new Float64Array(test1.buffer, 8, 2);
oneTest(test8);

var arr = [1,2,3,4,5,6,7,8,9,10,11,12];

WScript.Echo("test9");
var test9 = new Float64Array(arr);
oneTest(test9);

WScript.Echo("test9.1");
printObj(test1);
test9.set(test1);
oneTest(test9);

WScript.Echo("test9.2");
test9.set(test5);
oneTest(test9); 

WScript.Echo("test10");
try {
var test10 = new Float64Array({});
oneTest(test10);
}
catch(e)
{
WScript.Echo("succeed with catching" + e); 
}

WScript.Echo("test11");
try
{
var test11 = new Float64Array('abcdefg');
oneTest(test11);
}
catch(e)
{
WScript.Echo("succeed with catching" + e); 
}

WScript.Echo("test11.1");
var test111 = new Float64Array(new String('abcdefg'));
oneTest(test111);

WScript.Echo("test12");
var test12 = new Float64Array(0);
oneTest(test12);


WScript.Echo("test13");
var test13 = new Float64Array(arrayBuffer, 0);
oneTest(test13);


WScript.Echo("test14");
try 
{
var test14 = new Float64Array(arrayBuffer, 0, 0);
oneTest(test14);
}
catch(e)
{
WScript.Echo("succeed with catching" + e); 
}


WScript.Echo("test15");
try 
{
var test15 = new Float64Array(arrayBuffer, 0, 40);
oneTest(test15);
}
catch(e)
{
WScript.Echo("succeed with catching" + e); 
}

WScript.Echo("test16");
try 
{
var test16 = new Float64Array(arrayBuffer, 40, 4);
oneTest(test16);
}
catch(e)
{
WScript.Echo("succeed with catching" + e); 
}
printObj(test5);
WScript.Echo("test17");
var test17 = test5.subarray(0);
printObj(test17);

WScript.Echo("test18");
var test18 = test5.subarray(4);
printObj(test18);

WScript.Echo("test19");
var test19    = test5.subarray(0, 3);
printObj(test19);


WScript.Echo("test20");
var test20    = test5.subarray(0, 20);
printObj(test20);


WScript.Echo("test21");
var test21 = new test5.constructor(2);

try {
    WScript.Echo(Object.getPrototypeOf(test21));
} catch(e) {
    WScript.Echo(test21.BYTES_PER_ELEMENT);
}

printObj(test21);

WScript.Echo("test22");
WScript.Echo(Float64Array.prototype[10]);
WScript.Echo(Float64Array.prototype[-1]);
WScript.Echo(Float64Array.prototype[2]);
Float64Array.prototype[2] = 10;
WScript.Echo(Float64Array.prototype[2]);

WScript.Echo("test23");
testSetWithInt(-1, 2, new Float64Array(3), new Float64Array(3), new Float64Array(3));
testSetWithFloat(-1, 2, new Float64Array(3), new Float64Array(3), new Float64Array(3));
testSetWithObj(-1, 2, new Float64Array(3), new Float64Array(3), new Float64Array(3));

WScript.Echo("test23 JIT");
testSetWithInt(-1, 2, new Float64Array(3), new Float64Array(3), new Float64Array(3));
testSetWithFloat(-1, 2, new Float64Array(3), new Float64Array(3), new Float64Array(3));
testSetWithObj(-1, 2, new Float64Array(3), new Float64Array(3), new Float64Array(3));

WScript.Echo("test24");
testIndexValueForSet(new Float64Array(5));
WScript.Echo("test24 JIT");
testIndexValueForSet(new Float64Array(5));