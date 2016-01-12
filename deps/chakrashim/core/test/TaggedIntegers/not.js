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
result = (~ x);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1: argument is a constant
result = (~ 0);
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2: argument is a variable
x = 1;
result = (~ x);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3: argument is a constant
result = (~ 1);
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 4: argument is a variable
x = -1;
result = (~ x);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 5: argument is a constant
result = (~ -1);
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 6: argument is a variable
x = 2;
result = (~ x);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 7: argument is a constant
result = (~ 2);
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 8: argument is a variable
x = -2;
result = (~ x);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 9: argument is a constant
result = (~ -2);
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 10: argument is a variable
x = 3;
result = (~ x);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 11: argument is a constant
result = (~ 3);
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 12: argument is a variable
x = -3;
result = (~ x);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 13: argument is a constant
result = (~ -3);
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 14: argument is a variable
x = 4;
result = (~ x);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 15: argument is a constant
result = (~ 4);
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 16: argument is a variable
x = -4;
result = (~ x);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 17: argument is a constant
result = (~ -4);
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 18: argument is a variable
x = 8;
result = (~ x);
check = -9;
if(result != check) { fail(test, check, result); } ++test; 

// Test 19: argument is a constant
result = (~ 8);
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 20: argument is a variable
x = -8;
result = (~ x);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 21: argument is a constant
result = (~ -8);
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 22: argument is a variable
x = 1073741822;
result = (~ x);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 23: argument is a constant
result = (~ 1073741822);
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 24: argument is a variable
x = 1073741823;
result = (~ x);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 25: argument is a constant
result = (~ 1073741823);
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 26: argument is a variable
x = 1073741824;
result = (~ x);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 27: argument is a constant
result = (~ 1073741824);
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 28: argument is a variable
x = 1073741825;
result = (~ x);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 29: argument is a constant
result = (~ 1073741825);
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 30: argument is a variable
x = -1073741823;
result = (~ x);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 31: argument is a constant
result = (~ -1073741823);
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 32: argument is a variable
x = (-0x3fffffff-1);
result = (~ x);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 33: argument is a constant
result = (~ (-0x3fffffff-1));
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 34: argument is a variable
x = -1073741825;
result = (~ x);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 35: argument is a constant
result = (~ -1073741825);
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 36: argument is a variable
x = -1073741826;
result = (~ x);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 37: argument is a constant
result = (~ -1073741826);
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 38: argument is a variable
x = 2147483646;
result = (~ x);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 39: argument is a constant
result = (~ 2147483646);
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 40: argument is a variable
x = 2147483647;
result = (~ x);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 41: argument is a constant
result = (~ 2147483647);
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 42: argument is a variable
x = 2147483648;
result = (~ x);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 43: argument is a constant
result = (~ 2147483648);
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 44: argument is a variable
x = 2147483649;
result = (~ x);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 45: argument is a constant
result = (~ 2147483649);
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 46: argument is a variable
x = -2147483647;
result = (~ x);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 47: argument is a constant
result = (~ -2147483647);
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 48: argument is a variable
x = -2147483648;
result = (~ x);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 49: argument is a constant
result = (~ -2147483648);
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 50: argument is a variable
x = -2147483649;
result = (~ x);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 51: argument is a constant
result = (~ -2147483649);
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 52: argument is a variable
x = -2147483650;
result = (~ x);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 53: argument is a constant
result = (~ -2147483650);
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 54: argument is a variable
x = 4294967295;
result = (~ x);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 55: argument is a constant
result = (~ 4294967295);
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 56: argument is a variable
x = 4294967296;
result = (~ x);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 57: argument is a constant
result = (~ 4294967296);
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 58: argument is a variable
x = -4294967295;
result = (~ x);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 59: argument is a constant
result = (~ -4294967295);
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 60: argument is a variable
x = -4294967296;
result = (~ x);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 61: argument is a constant
result = (~ -4294967296);
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

}
test0();
WScript.Echo("done");
