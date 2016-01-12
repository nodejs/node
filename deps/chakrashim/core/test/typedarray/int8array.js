//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("util.js");

function oneTest(a)
{

a[5] = 10;
a[1] = 1.5;
a[2]= 0x80;
a[-1] = 2;
a["-3"] = 4;
WScript.Echo(a[5]);
if (Object.getOwnPropertyDescriptor(a, 100000) != undefined) {
    WScript.Echo('FAIL');
}

try {
    var pro = Int8Array.prototype;
    WScript.Echo(pro.toString());

    WScript.Echo("prototype is");
    printObj(pro);
} catch(e) {
    WScript.Echo("constructor is");
    printObj(Int8Array);
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
var test1 = new Int8Array(9);
oneTest(test1);
WScript.Echo("test2");
var test2 = new Int8Array(0);
oneTest(test2);

WScript.Echo("test3");
var arrayBuffer = new ArrayBuffer(30);
var test3 = new Int8Array(arrayBuffer);
oneTest(test3);

WScript.Echo("test4");
var test4 = new Int8Array(arrayBuffer, 3);
oneTest(test4);

WScript.Echo("test5");
var test5 = new Int8Array(arrayBuffer, 3, 6);
oneTest(test5);

WScript.Echo("test6");
var mybuffer = test1.buffer;
WScript.Echo(mybuffer);
var test6 = new Int8Array(mybuffer);
oneTest(test6);

WScript.Echo("test7");
var test7 = new Int8Array(test1.buffer, 3);
oneTest(test7);

WScript.Echo("test8");
var test8 = new Int8Array(test1.buffer, 2, 6);
oneTest(test8);

var arr = [1,2,3,4,5,6,7,8,9,10,11,12];

WScript.Echo("test9");
var test9 = new Int8Array(arr);
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
var test10 = new Int8Array({});
oneTest(test10);
}
catch(e)
{
WScript.Echo("succeed with catching" + e);
}

WScript.Echo("test11");
try {
var test11 = new Int8Array('abcdefg');
oneTest(test11);
}
catch(e)
{
WScript.Echo("succeed with catching" + e);
}

WScript.Echo("test11.1");
var test111 = new Int8Array(new String('abcdefg'));
oneTest(test111);

WScript.Echo("test12");
var test12 = new Int8Array(0);
oneTest(test12);

WScript.Echo("test13");
var test13 = new Int8Array(arrayBuffer, 0);
oneTest(test13);

WScript.Echo("test14");
try
{
var test14 = new Int8Array(arrayBuffer, 0, 0);
oneTest(test14);
}
catch(e)
{
WScript.Echo("succeed with catching" + e);
}

WScript.Echo("test15");
try
{
var test15 = new Int8Array(arrayBuffer, 0, 40);
oneTest(test15);
}
 catch(e)
{
WScript.Echo("succeed with catching" + e);
}

WScript.Echo("test16");
try
{
var test16 = new Int8Array(arrayBuffer, 40, 4);
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
var test20 = test5.subarray(-3, -1);
printObj(test20);

WScript.Echo("test21");
var test21 = test5.subarray(0, -2);
printObj(test21);

WScript.Echo("test22");
var test22 = test5.subarray(-100, -100);
printObj(test22);

WScript.Echo("test23");
var test23 = test5.subarray(1);
printObj(test23);

WScript.Echo("test24");
var test24 = test5.subarray(1, 3);
printObj(test24);

var arr;
print("test23: constructor");
    var validSizeValues = ["abc",
                            "123",
                              Infinity,
                              -Infinity,
                            ];
        for (var i = 0; i < validSizeValues.length; i++) {
            var size = validSizeValues[i];
        print("size is" + size);
            verifyNoThrow(function(){arr = new Int8Array(size);});
            printObj(arr);
    }

    var invalidSizeValues = [ undefined,
                              -1,
                              new Object(),
                              new Number(NaN),
                            ];

        for (var i = 0; i < validSizeValues.length; i++) {
            var size = invalidSizeValues[i];
            verifyThrow(function(){arr = new Int8Array(size);});
    }

WScript.Echo("test24");
WScript.Echo(Int8Array.prototype[10]);
WScript.Echo(Int8Array.prototype[-1]);
WScript.Echo(Int8Array.prototype[2]);
Int8Array.prototype[2] = 10;
WScript.Echo(Int8Array.prototype[2]);

WScript.Echo("test25");
testSetWithInt(-1, 2, new Int8Array(3), new Int8Array(3), new Int8Array(3));
testSetWithFloat(-1, 2, new Int8Array(3), new Int8Array(3), new Int8Array(3));
testSetWithObj(-1, 2, new Int8Array(3), new Int8Array(3), new Int8Array(3));

WScript.Echo("test25 JIT");
testSetWithInt(-1, 2, new Int8Array(3), new Int8Array(3), new Int8Array(3));
testSetWithFloat(-1, 2, new Int8Array(3), new Int8Array(3), new Int8Array(3));
testSetWithObj(-1, 2, new Int8Array(3), new Int8Array(3), new Int8Array(3));

WScript.Echo("test26");
testIndexValueForSet(new Int8Array(5));
WScript.Echo("test26 JIT");
testIndexValueForSet(new Int8Array(5));
