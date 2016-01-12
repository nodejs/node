//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var n1 = new Number(10);
n1.toString = function() { return 20; }

var n2 = new Number(30);
n2.valueOf = function() { return 40; }

var n3 = new Number(50);
n3.toString = function() { return 60; }
n3.valueOf  = function() { return 70; }

var d1 = new Date(1974, 9, 24, 0, 20, 30, 40, 50);

var a1 = [ 10, 20 ];
a1.toString = function() { return "array a1"; }

var a2 = [ 10.123, 20.456 ];

var values = [
    0, 1, -1, 
    12345678, 10.23344, -1.2345,
    NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number(11111111222),
    "hello", new String("hello" + "world"),
    false, new Boolean(true),
    new Object(),
    n1, n2, n3,
    d1,
    a1, a2,
    12345678912345678,
    1
];

var v;
for (var i=0;i<values.length; i++)
{
    v = values[i];
    write(i + " toString()     : " + v.toString());
    write(i + " toLocaleString : " + v.toLocaleString());    
}

var arr = [1, values, null, undefined, , 20];

arr[arr.length] = arr;
arr[arr.length] = "LastValue!!";

write("arr.toString()     : " + arr.toString());
write("arr.toLocaleString : " + arr.toLocaleString());

var arr1 = new Array (7) ;
write("arr1.toString()     : " + arr1.toString());
write("arr1.toLocaleString : " + arr1.toLocaleString());