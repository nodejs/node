//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var test = 1; 
function fail(n, expected, result) { WScript.Echo("failure in test " + test + "; expected " + expected + ", got " + result); }
function test0() {
var x;
var y;
var result;
var check;
// Test 0: argument is a variable
x = 0;
result = (-- x);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1: argument is a variable
x = 1;
result = (-- x);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2: argument is a variable
x = -1;
result = (-- x);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3: argument is a variable
x = 2;
result = (-- x);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 4: argument is a variable
x = -2;
result = (-- x);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 5: argument is a variable
x = 3;
result = (-- x);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 6: argument is a variable
x = -3;
result = (-- x);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 7: argument is a variable
x = 4;
result = (-- x);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 8: argument is a variable
x = -4;
result = (-- x);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 9: argument is a variable
x = 8;
result = (-- x);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 10: argument is a variable
x = -8;
result = (-- x);
check = -9;
if(result != check) { fail(test, check, result); } ++test; 

// Test 11: argument is a variable
x = 1073741822;
result = (-- x);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 12: argument is a variable
x = 1073741823;
result = (-- x);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 13: argument is a variable
x = 1073741824;
result = (-- x);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 14: argument is a variable
x = 1073741825;
result = (-- x);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 15: argument is a variable
x = -1073741823;
result = (-- x);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 16: argument is a variable
x = (-0x3fffffff-1);
result = (-- x);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 17: argument is a variable
x = -1073741825;
result = (-- x);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 18: argument is a variable
x = -1073741826;
result = (-- x);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 19: argument is a variable
x = 2147483646;
result = (-- x);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 20: argument is a variable
x = 2147483647;
result = (-- x);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 21: argument is a variable
x = 2147483648;
result = (-- x);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 22: argument is a variable
x = 2147483649;
result = (-- x);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 23: argument is a variable
x = -2147483647;
result = (-- x);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 24: argument is a variable
x = -2147483648;
result = (-- x);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 25: argument is a variable
x = -2147483649;
result = (-- x);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 26: argument is a variable
x = -2147483650;
result = (-- x);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 27: argument is a variable
x = 4294967295;
result = (-- x);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 28: argument is a variable
x = 4294967296;
result = (-- x);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 29: argument is a variable
x = -4294967295;
result = (-- x);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 30: argument is a variable
x = -4294967296;
result = (-- x);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

}
test0();
WScript.Echo("done");
