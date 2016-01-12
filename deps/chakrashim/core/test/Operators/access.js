//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var count = 1000;

write("Object................ ");

Object.prototype.o1     = count++;
Object.prototype[100]   = count++;

var obj = new Object();

write("obj.o1   : " + obj.o1);
write("obj[100] : " + obj[100]);


write("Array ................ ");
Array.prototype.a1      = count++;
Array.prototype[200]    = count++;

var arr = new Array(10);

//arr[-10]      = count++;
arr[0]        = count++;
arr[0.5]      = count++;
arr[1]        = count++;
arr[5]        = count++;
arr["6"]      = count++;
arr["7.0"]    = count++;
arr["8.2"]    = count++;
arr[NaN]      = count++;
arr[Infinity] = count++;

write("arr.o1        : " + arr.o1);
write("arr.a1        : " + arr.a1);
write("arr[100]      : " + arr[100]);
write("arr[200]      : " + arr[200]);

write("arr[0]        : " + arr[0]);
write("arr[0.5]      : " + arr[0.5]);
write("arr[\"0.5\"]  : " + arr["0.5"]);
write("arr[1]        : " + arr[1]);
write("arr[\"1\"]    : " + arr["1"]);
write("arr[5]        : " + arr[5]);
write("arr[6]        : " + arr[6]);
write("arr[\"6\"]    : " + arr["6"]);
write("arr[7]        : " + arr[7]);
write("arr[7.0]      : " + arr[7.0]);
write("arr[\"7.0\"]  : " + arr["7.0"]);
write("arr[8.2]      : " + arr[8.2]);
write("arr[\"8.2\"]  : " + arr["8.2"]);
write("arr[NaN]      : " + arr[NaN]);
write("arr[Infinity] : " + arr[Infinity]);

write("String................ ");

String.prototype.s1     = count++;
String.prototype[300]   = count++;

var str = new String("Welcome");

str[0]        = count++;
str[0.5]      = count++;
str[1]        = count++;
str[5]        = count++;
str["6"]      = count++;
str["7.0"]    = count++;
str["8.2"]    = count++;
str[10] = count++;
str[50] = count++;


write("str.o1 : " + str.o1);
write("str.s1 : " + str.s1);
write("str[100]      : " + str[100]);
write("str[200]      : " + str[200]);
write("str[0]        : " + str[0]);
write("str[0.5]      : " + str[0.5]);
write("str[\"0.5\"]  : " + str["0.5"]);
write("str[1]        : " + str[1]);
write("str[\"1\"]    : " + str["1"]);
write("str[5]        : " + str[5]);
write("str[6]        : " + str[6]);
write("str[\"6\"]    : " + str["6"]);
write("str[7]        : " + str[7]);
write("str[7.0]      : " + str[7.0]);
write("str[\"7.0\"]  : " + str["7.0"]);
write("str[8.2]      : " + str[8.2]);
write("str[\"8.2\"]  : " + str["8.2"]);


write("Function.............. ");
Function.prototype.f1   = count++;
Function.prototype[400] = count++;


function fun() {
    return 1;
}

write("fun.o1 : " + fun.o1);
write("fun.f1 : " + fun.f1);
write("fun[100] : " + fun[100]);
write("fun[400] : " + fun[400]);


function fun1() {};
var arr1 = new Array();
arr1[10] = count++;

fun1.prototype = arr1;

var fun1Instance = new fun1();

write("fun1Instance.o1   : " + fun1Instance.o1);
write("fun1Instance.a1   : " + fun1Instance.a1);
write("fun1Instance[100] : " + fun1Instance[100]);
write("fun1Instance[200] : " + fun1Instance[200]);
write("fun1Instance[10]  : " + fun1Instance[10]);
