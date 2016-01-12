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
// Test 0: both arguments variables
x = 0;
y = 0;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1: both arguments constants
result = (0 - 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2: LHS constant
y = 0;
result = (0 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3: RHS constant
x = 0;
result = (x - 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 4: both arguments variables
x = 0;
y = 1;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 5: both arguments constants
result = (0 - 1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 6: LHS constant
y = 1;
result = (0 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 7: RHS constant
x = 0;
result = (x - 1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 8: both arguments variables
x = 0;
y = -1;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 9: both arguments constants
result = (0 - -1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 10: LHS constant
y = -1;
result = (0 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 11: RHS constant
x = 0;
result = (x - -1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 12: both arguments variables
x = 0;
y = 2;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 13: both arguments constants
result = (0 - 2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 14: LHS constant
y = 2;
result = (0 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 15: RHS constant
x = 0;
result = (x - 2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 16: both arguments variables
x = 0;
y = -2;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 17: both arguments constants
result = (0 - -2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 18: LHS constant
y = -2;
result = (0 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 19: RHS constant
x = 0;
result = (x - -2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 20: both arguments variables
x = 0;
y = 3;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 21: both arguments constants
result = (0 - 3)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 22: LHS constant
y = 3;
result = (0 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 23: RHS constant
x = 0;
result = (x - 3)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 24: both arguments variables
x = 0;
y = -3;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 25: both arguments constants
result = (0 - -3)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 26: LHS constant
y = -3;
result = (0 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 27: RHS constant
x = 0;
result = (x - -3)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 28: both arguments variables
x = 0;
y = 4;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 29: both arguments constants
result = (0 - 4)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 30: LHS constant
y = 4;
result = (0 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 31: RHS constant
x = 0;
result = (x - 4)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 32: both arguments variables
x = 0;
y = -4;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 33: both arguments constants
result = (0 - -4)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 34: LHS constant
y = -4;
result = (0 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 35: RHS constant
x = 0;
result = (x - -4)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 36: both arguments variables
x = 0;
y = 8;
result = (x - y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 37: both arguments constants
result = (0 - 8)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 38: LHS constant
y = 8;
result = (0 - y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 39: RHS constant
x = 0;
result = (x - 8)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 40: both arguments variables
x = 0;
y = -8;
result = (x - y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 41: both arguments constants
result = (0 - -8)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 42: LHS constant
y = -8;
result = (0 - y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 43: RHS constant
x = 0;
result = (x - -8)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 44: both arguments variables
x = 0;
y = 1073741822;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 45: both arguments constants
result = (0 - 1073741822)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 46: LHS constant
y = 1073741822;
result = (0 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 47: RHS constant
x = 0;
result = (x - 1073741822)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 48: both arguments variables
x = 0;
y = 1073741823;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 49: both arguments constants
result = (0 - 1073741823)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 50: LHS constant
y = 1073741823;
result = (0 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 51: RHS constant
x = 0;
result = (x - 1073741823)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 52: both arguments variables
x = 0;
y = 1073741824;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 53: both arguments constants
result = (0 - 1073741824)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 54: LHS constant
y = 1073741824;
result = (0 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 55: RHS constant
x = 0;
result = (x - 1073741824)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 56: both arguments variables
x = 0;
y = 1073741825;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 57: both arguments constants
result = (0 - 1073741825)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 58: LHS constant
y = 1073741825;
result = (0 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 59: RHS constant
x = 0;
result = (x - 1073741825)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 60: both arguments variables
x = 0;
y = -1073741823;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 61: both arguments constants
result = (0 - -1073741823)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 62: LHS constant
y = -1073741823;
result = (0 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 63: RHS constant
x = 0;
result = (x - -1073741823)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 64: both arguments variables
x = 0;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 65: both arguments constants
result = (0 - (-0x3fffffff-1))
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 66: LHS constant
y = (-0x3fffffff-1);
result = (0 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 67: RHS constant
x = 0;
result = (x - (-0x3fffffff-1))
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 68: both arguments variables
x = 0;
y = -1073741825;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 69: both arguments constants
result = (0 - -1073741825)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 70: LHS constant
y = -1073741825;
result = (0 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 71: RHS constant
x = 0;
result = (x - -1073741825)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 72: both arguments variables
x = 0;
y = -1073741826;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 73: both arguments constants
result = (0 - -1073741826)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 74: LHS constant
y = -1073741826;
result = (0 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 75: RHS constant
x = 0;
result = (x - -1073741826)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 76: both arguments variables
x = 0;
y = 2147483646;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 77: both arguments constants
result = (0 - 2147483646)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 78: LHS constant
y = 2147483646;
result = (0 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 79: RHS constant
x = 0;
result = (x - 2147483646)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 80: both arguments variables
x = 0;
y = 2147483647;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 81: both arguments constants
result = (0 - 2147483647)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 82: LHS constant
y = 2147483647;
result = (0 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 83: RHS constant
x = 0;
result = (x - 2147483647)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 84: both arguments variables
x = 0;
y = 2147483648;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 85: both arguments constants
result = (0 - 2147483648)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 86: LHS constant
y = 2147483648;
result = (0 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 87: RHS constant
x = 0;
result = (x - 2147483648)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 88: both arguments variables
x = 0;
y = 2147483649;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 89: both arguments constants
result = (0 - 2147483649)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 90: LHS constant
y = 2147483649;
result = (0 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 91: RHS constant
x = 0;
result = (x - 2147483649)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 92: both arguments variables
x = 0;
y = -2147483647;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 93: both arguments constants
result = (0 - -2147483647)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 94: LHS constant
y = -2147483647;
result = (0 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 95: RHS constant
x = 0;
result = (x - -2147483647)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 96: both arguments variables
x = 0;
y = -2147483648;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 97: both arguments constants
result = (0 - -2147483648)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 98: LHS constant
y = -2147483648;
result = (0 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 99: RHS constant
x = 0;
result = (x - -2147483648)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test1() {
var x;
var y;
var result;
var check;
// Test 100: both arguments variables
x = 0;
y = -2147483649;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 101: both arguments constants
result = (0 - -2147483649)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 102: LHS constant
y = -2147483649;
result = (0 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 103: RHS constant
x = 0;
result = (x - -2147483649)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 104: both arguments variables
x = 0;
y = -2147483650;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 105: both arguments constants
result = (0 - -2147483650)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 106: LHS constant
y = -2147483650;
result = (0 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 107: RHS constant
x = 0;
result = (x - -2147483650)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 108: both arguments variables
x = 0;
y = 4294967295;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 109: both arguments constants
result = (0 - 4294967295)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 110: LHS constant
y = 4294967295;
result = (0 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 111: RHS constant
x = 0;
result = (x - 4294967295)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 112: both arguments variables
x = 0;
y = 4294967296;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 113: both arguments constants
result = (0 - 4294967296)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 114: LHS constant
y = 4294967296;
result = (0 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 115: RHS constant
x = 0;
result = (x - 4294967296)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 116: both arguments variables
x = 0;
y = -4294967295;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 117: both arguments constants
result = (0 - -4294967295)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 118: LHS constant
y = -4294967295;
result = (0 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 119: RHS constant
x = 0;
result = (x - -4294967295)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 120: both arguments variables
x = 0;
y = -4294967296;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 121: both arguments constants
result = (0 - -4294967296)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 122: LHS constant
y = -4294967296;
result = (0 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 123: RHS constant
x = 0;
result = (x - -4294967296)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 124: both arguments variables
x = 1;
y = 0;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 125: both arguments constants
result = (1 - 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 126: LHS constant
y = 0;
result = (1 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 127: RHS constant
x = 1;
result = (x - 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 128: both arguments variables
x = 1;
y = 1;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 129: both arguments constants
result = (1 - 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 130: LHS constant
y = 1;
result = (1 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 131: RHS constant
x = 1;
result = (x - 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 132: both arguments variables
x = 1;
y = -1;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 133: both arguments constants
result = (1 - -1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 134: LHS constant
y = -1;
result = (1 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 135: RHS constant
x = 1;
result = (x - -1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 136: both arguments variables
x = 1;
y = 2;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 137: both arguments constants
result = (1 - 2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 138: LHS constant
y = 2;
result = (1 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 139: RHS constant
x = 1;
result = (x - 2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 140: both arguments variables
x = 1;
y = -2;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 141: both arguments constants
result = (1 - -2)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 142: LHS constant
y = -2;
result = (1 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 143: RHS constant
x = 1;
result = (x - -2)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 144: both arguments variables
x = 1;
y = 3;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 145: both arguments constants
result = (1 - 3)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 146: LHS constant
y = 3;
result = (1 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 147: RHS constant
x = 1;
result = (x - 3)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 148: both arguments variables
x = 1;
y = -3;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 149: both arguments constants
result = (1 - -3)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 150: LHS constant
y = -3;
result = (1 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 151: RHS constant
x = 1;
result = (x - -3)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 152: both arguments variables
x = 1;
y = 4;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 153: both arguments constants
result = (1 - 4)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 154: LHS constant
y = 4;
result = (1 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 155: RHS constant
x = 1;
result = (x - 4)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 156: both arguments variables
x = 1;
y = -4;
result = (x - y);
check = 5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 157: both arguments constants
result = (1 - -4)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 158: LHS constant
y = -4;
result = (1 - y)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 159: RHS constant
x = 1;
result = (x - -4)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 160: both arguments variables
x = 1;
y = 8;
result = (x - y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 161: both arguments constants
result = (1 - 8)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 162: LHS constant
y = 8;
result = (1 - y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 163: RHS constant
x = 1;
result = (x - 8)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 164: both arguments variables
x = 1;
y = -8;
result = (x - y);
check = 9;
if(result != check) { fail(test, check, result); } ++test; 

// Test 165: both arguments constants
result = (1 - -8)
check = 9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 166: LHS constant
y = -8;
result = (1 - y)
check = 9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 167: RHS constant
x = 1;
result = (x - -8)
check = 9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 168: both arguments variables
x = 1;
y = 1073741822;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 169: both arguments constants
result = (1 - 1073741822)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 170: LHS constant
y = 1073741822;
result = (1 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 171: RHS constant
x = 1;
result = (x - 1073741822)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 172: both arguments variables
x = 1;
y = 1073741823;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 173: both arguments constants
result = (1 - 1073741823)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 174: LHS constant
y = 1073741823;
result = (1 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 175: RHS constant
x = 1;
result = (x - 1073741823)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 176: both arguments variables
x = 1;
y = 1073741824;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 177: both arguments constants
result = (1 - 1073741824)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 178: LHS constant
y = 1073741824;
result = (1 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 179: RHS constant
x = 1;
result = (x - 1073741824)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 180: both arguments variables
x = 1;
y = 1073741825;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 181: both arguments constants
result = (1 - 1073741825)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 182: LHS constant
y = 1073741825;
result = (1 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 183: RHS constant
x = 1;
result = (x - 1073741825)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 184: both arguments variables
x = 1;
y = -1073741823;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 185: both arguments constants
result = (1 - -1073741823)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 186: LHS constant
y = -1073741823;
result = (1 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 187: RHS constant
x = 1;
result = (x - -1073741823)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 188: both arguments variables
x = 1;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 189: both arguments constants
result = (1 - (-0x3fffffff-1))
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 190: LHS constant
y = (-0x3fffffff-1);
result = (1 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 191: RHS constant
x = 1;
result = (x - (-0x3fffffff-1))
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 192: both arguments variables
x = 1;
y = -1073741825;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 193: both arguments constants
result = (1 - -1073741825)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 194: LHS constant
y = -1073741825;
result = (1 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 195: RHS constant
x = 1;
result = (x - -1073741825)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 196: both arguments variables
x = 1;
y = -1073741826;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 197: both arguments constants
result = (1 - -1073741826)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 198: LHS constant
y = -1073741826;
result = (1 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 199: RHS constant
x = 1;
result = (x - -1073741826)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test2() {
var x;
var y;
var result;
var check;
// Test 200: both arguments variables
x = 1;
y = 2147483646;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 201: both arguments constants
result = (1 - 2147483646)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 202: LHS constant
y = 2147483646;
result = (1 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 203: RHS constant
x = 1;
result = (x - 2147483646)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 204: both arguments variables
x = 1;
y = 2147483647;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 205: both arguments constants
result = (1 - 2147483647)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 206: LHS constant
y = 2147483647;
result = (1 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 207: RHS constant
x = 1;
result = (x - 2147483647)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 208: both arguments variables
x = 1;
y = 2147483648;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 209: both arguments constants
result = (1 - 2147483648)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 210: LHS constant
y = 2147483648;
result = (1 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 211: RHS constant
x = 1;
result = (x - 2147483648)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 212: both arguments variables
x = 1;
y = 2147483649;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 213: both arguments constants
result = (1 - 2147483649)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 214: LHS constant
y = 2147483649;
result = (1 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 215: RHS constant
x = 1;
result = (x - 2147483649)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 216: both arguments variables
x = 1;
y = -2147483647;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 217: both arguments constants
result = (1 - -2147483647)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 218: LHS constant
y = -2147483647;
result = (1 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 219: RHS constant
x = 1;
result = (x - -2147483647)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 220: both arguments variables
x = 1;
y = -2147483648;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 221: both arguments constants
result = (1 - -2147483648)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 222: LHS constant
y = -2147483648;
result = (1 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 223: RHS constant
x = 1;
result = (x - -2147483648)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 224: both arguments variables
x = 1;
y = -2147483649;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 225: both arguments constants
result = (1 - -2147483649)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 226: LHS constant
y = -2147483649;
result = (1 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 227: RHS constant
x = 1;
result = (x - -2147483649)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 228: both arguments variables
x = 1;
y = -2147483650;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 229: both arguments constants
result = (1 - -2147483650)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 230: LHS constant
y = -2147483650;
result = (1 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 231: RHS constant
x = 1;
result = (x - -2147483650)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 232: both arguments variables
x = 1;
y = 4294967295;
result = (x - y);
check = -4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 233: both arguments constants
result = (1 - 4294967295)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 234: LHS constant
y = 4294967295;
result = (1 - y)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 235: RHS constant
x = 1;
result = (x - 4294967295)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 236: both arguments variables
x = 1;
y = 4294967296;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 237: both arguments constants
result = (1 - 4294967296)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 238: LHS constant
y = 4294967296;
result = (1 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 239: RHS constant
x = 1;
result = (x - 4294967296)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 240: both arguments variables
x = 1;
y = -4294967295;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 241: both arguments constants
result = (1 - -4294967295)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 242: LHS constant
y = -4294967295;
result = (1 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 243: RHS constant
x = 1;
result = (x - -4294967295)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 244: both arguments variables
x = 1;
y = -4294967296;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 245: both arguments constants
result = (1 - -4294967296)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 246: LHS constant
y = -4294967296;
result = (1 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 247: RHS constant
x = 1;
result = (x - -4294967296)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 248: both arguments variables
x = -1;
y = 0;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 249: both arguments constants
result = (-1 - 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 250: LHS constant
y = 0;
result = (-1 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 251: RHS constant
x = -1;
result = (x - 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 252: both arguments variables
x = -1;
y = 1;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 253: both arguments constants
result = (-1 - 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 254: LHS constant
y = 1;
result = (-1 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 255: RHS constant
x = -1;
result = (x - 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 256: both arguments variables
x = -1;
y = -1;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 257: both arguments constants
result = (-1 - -1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 258: LHS constant
y = -1;
result = (-1 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 259: RHS constant
x = -1;
result = (x - -1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 260: both arguments variables
x = -1;
y = 2;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 261: both arguments constants
result = (-1 - 2)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 262: LHS constant
y = 2;
result = (-1 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 263: RHS constant
x = -1;
result = (x - 2)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 264: both arguments variables
x = -1;
y = -2;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 265: both arguments constants
result = (-1 - -2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 266: LHS constant
y = -2;
result = (-1 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 267: RHS constant
x = -1;
result = (x - -2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 268: both arguments variables
x = -1;
y = 3;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 269: both arguments constants
result = (-1 - 3)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 270: LHS constant
y = 3;
result = (-1 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 271: RHS constant
x = -1;
result = (x - 3)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 272: both arguments variables
x = -1;
y = -3;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 273: both arguments constants
result = (-1 - -3)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 274: LHS constant
y = -3;
result = (-1 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 275: RHS constant
x = -1;
result = (x - -3)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 276: both arguments variables
x = -1;
y = 4;
result = (x - y);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 277: both arguments constants
result = (-1 - 4)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 278: LHS constant
y = 4;
result = (-1 - y)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 279: RHS constant
x = -1;
result = (x - 4)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 280: both arguments variables
x = -1;
y = -4;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 281: both arguments constants
result = (-1 - -4)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 282: LHS constant
y = -4;
result = (-1 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 283: RHS constant
x = -1;
result = (x - -4)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 284: both arguments variables
x = -1;
y = 8;
result = (x - y);
check = -9;
if(result != check) { fail(test, check, result); } ++test; 

// Test 285: both arguments constants
result = (-1 - 8)
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 286: LHS constant
y = 8;
result = (-1 - y)
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 287: RHS constant
x = -1;
result = (x - 8)
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 288: both arguments variables
x = -1;
y = -8;
result = (x - y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 289: both arguments constants
result = (-1 - -8)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 290: LHS constant
y = -8;
result = (-1 - y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 291: RHS constant
x = -1;
result = (x - -8)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 292: both arguments variables
x = -1;
y = 1073741822;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 293: both arguments constants
result = (-1 - 1073741822)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 294: LHS constant
y = 1073741822;
result = (-1 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 295: RHS constant
x = -1;
result = (x - 1073741822)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 296: both arguments variables
x = -1;
y = 1073741823;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 297: both arguments constants
result = (-1 - 1073741823)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 298: LHS constant
y = 1073741823;
result = (-1 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 299: RHS constant
x = -1;
result = (x - 1073741823)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test3() {
var x;
var y;
var result;
var check;
// Test 300: both arguments variables
x = -1;
y = 1073741824;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 301: both arguments constants
result = (-1 - 1073741824)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 302: LHS constant
y = 1073741824;
result = (-1 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 303: RHS constant
x = -1;
result = (x - 1073741824)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 304: both arguments variables
x = -1;
y = 1073741825;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 305: both arguments constants
result = (-1 - 1073741825)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 306: LHS constant
y = 1073741825;
result = (-1 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 307: RHS constant
x = -1;
result = (x - 1073741825)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 308: both arguments variables
x = -1;
y = -1073741823;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 309: both arguments constants
result = (-1 - -1073741823)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 310: LHS constant
y = -1073741823;
result = (-1 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 311: RHS constant
x = -1;
result = (x - -1073741823)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 312: both arguments variables
x = -1;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 313: both arguments constants
result = (-1 - (-0x3fffffff-1))
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 314: LHS constant
y = (-0x3fffffff-1);
result = (-1 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 315: RHS constant
x = -1;
result = (x - (-0x3fffffff-1))
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 316: both arguments variables
x = -1;
y = -1073741825;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 317: both arguments constants
result = (-1 - -1073741825)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 318: LHS constant
y = -1073741825;
result = (-1 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 319: RHS constant
x = -1;
result = (x - -1073741825)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 320: both arguments variables
x = -1;
y = -1073741826;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 321: both arguments constants
result = (-1 - -1073741826)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 322: LHS constant
y = -1073741826;
result = (-1 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 323: RHS constant
x = -1;
result = (x - -1073741826)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 324: both arguments variables
x = -1;
y = 2147483646;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 325: both arguments constants
result = (-1 - 2147483646)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 326: LHS constant
y = 2147483646;
result = (-1 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 327: RHS constant
x = -1;
result = (x - 2147483646)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 328: both arguments variables
x = -1;
y = 2147483647;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 329: both arguments constants
result = (-1 - 2147483647)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 330: LHS constant
y = 2147483647;
result = (-1 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 331: RHS constant
x = -1;
result = (x - 2147483647)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 332: both arguments variables
x = -1;
y = 2147483648;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 333: both arguments constants
result = (-1 - 2147483648)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 334: LHS constant
y = 2147483648;
result = (-1 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 335: RHS constant
x = -1;
result = (x - 2147483648)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 336: both arguments variables
x = -1;
y = 2147483649;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 337: both arguments constants
result = (-1 - 2147483649)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 338: LHS constant
y = 2147483649;
result = (-1 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 339: RHS constant
x = -1;
result = (x - 2147483649)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 340: both arguments variables
x = -1;
y = -2147483647;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 341: both arguments constants
result = (-1 - -2147483647)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 342: LHS constant
y = -2147483647;
result = (-1 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 343: RHS constant
x = -1;
result = (x - -2147483647)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 344: both arguments variables
x = -1;
y = -2147483648;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 345: both arguments constants
result = (-1 - -2147483648)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 346: LHS constant
y = -2147483648;
result = (-1 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 347: RHS constant
x = -1;
result = (x - -2147483648)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 348: both arguments variables
x = -1;
y = -2147483649;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 349: both arguments constants
result = (-1 - -2147483649)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 350: LHS constant
y = -2147483649;
result = (-1 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 351: RHS constant
x = -1;
result = (x - -2147483649)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 352: both arguments variables
x = -1;
y = -2147483650;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 353: both arguments constants
result = (-1 - -2147483650)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 354: LHS constant
y = -2147483650;
result = (-1 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 355: RHS constant
x = -1;
result = (x - -2147483650)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 356: both arguments variables
x = -1;
y = 4294967295;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 357: both arguments constants
result = (-1 - 4294967295)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 358: LHS constant
y = 4294967295;
result = (-1 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 359: RHS constant
x = -1;
result = (x - 4294967295)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 360: both arguments variables
x = -1;
y = 4294967296;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 361: both arguments constants
result = (-1 - 4294967296)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 362: LHS constant
y = 4294967296;
result = (-1 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 363: RHS constant
x = -1;
result = (x - 4294967296)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 364: both arguments variables
x = -1;
y = -4294967295;
result = (x - y);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 365: both arguments constants
result = (-1 - -4294967295)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 366: LHS constant
y = -4294967295;
result = (-1 - y)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 367: RHS constant
x = -1;
result = (x - -4294967295)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 368: both arguments variables
x = -1;
y = -4294967296;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 369: both arguments constants
result = (-1 - -4294967296)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 370: LHS constant
y = -4294967296;
result = (-1 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 371: RHS constant
x = -1;
result = (x - -4294967296)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 372: both arguments variables
x = 2;
y = 0;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 373: both arguments constants
result = (2 - 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 374: LHS constant
y = 0;
result = (2 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 375: RHS constant
x = 2;
result = (x - 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 376: both arguments variables
x = 2;
y = 1;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 377: both arguments constants
result = (2 - 1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 378: LHS constant
y = 1;
result = (2 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 379: RHS constant
x = 2;
result = (x - 1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 380: both arguments variables
x = 2;
y = -1;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 381: both arguments constants
result = (2 - -1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 382: LHS constant
y = -1;
result = (2 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 383: RHS constant
x = 2;
result = (x - -1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 384: both arguments variables
x = 2;
y = 2;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 385: both arguments constants
result = (2 - 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 386: LHS constant
y = 2;
result = (2 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 387: RHS constant
x = 2;
result = (x - 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 388: both arguments variables
x = 2;
y = -2;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 389: both arguments constants
result = (2 - -2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 390: LHS constant
y = -2;
result = (2 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 391: RHS constant
x = 2;
result = (x - -2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 392: both arguments variables
x = 2;
y = 3;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 393: both arguments constants
result = (2 - 3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 394: LHS constant
y = 3;
result = (2 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 395: RHS constant
x = 2;
result = (x - 3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 396: both arguments variables
x = 2;
y = -3;
result = (x - y);
check = 5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 397: both arguments constants
result = (2 - -3)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 398: LHS constant
y = -3;
result = (2 - y)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 399: RHS constant
x = 2;
result = (x - -3)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test4() {
var x;
var y;
var result;
var check;
// Test 400: both arguments variables
x = 2;
y = 4;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 401: both arguments constants
result = (2 - 4)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 402: LHS constant
y = 4;
result = (2 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 403: RHS constant
x = 2;
result = (x - 4)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 404: both arguments variables
x = 2;
y = -4;
result = (x - y);
check = 6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 405: both arguments constants
result = (2 - -4)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 406: LHS constant
y = -4;
result = (2 - y)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 407: RHS constant
x = 2;
result = (x - -4)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 408: both arguments variables
x = 2;
y = 8;
result = (x - y);
check = -6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 409: both arguments constants
result = (2 - 8)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 410: LHS constant
y = 8;
result = (2 - y)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 411: RHS constant
x = 2;
result = (x - 8)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 412: both arguments variables
x = 2;
y = -8;
result = (x - y);
check = 10;
if(result != check) { fail(test, check, result); } ++test; 

// Test 413: both arguments constants
result = (2 - -8)
check = 10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 414: LHS constant
y = -8;
result = (2 - y)
check = 10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 415: RHS constant
x = 2;
result = (x - -8)
check = 10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 416: both arguments variables
x = 2;
y = 1073741822;
result = (x - y);
check = -1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 417: both arguments constants
result = (2 - 1073741822)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 418: LHS constant
y = 1073741822;
result = (2 - y)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 419: RHS constant
x = 2;
result = (x - 1073741822)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 420: both arguments variables
x = 2;
y = 1073741823;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 421: both arguments constants
result = (2 - 1073741823)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 422: LHS constant
y = 1073741823;
result = (2 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 423: RHS constant
x = 2;
result = (x - 1073741823)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 424: both arguments variables
x = 2;
y = 1073741824;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 425: both arguments constants
result = (2 - 1073741824)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 426: LHS constant
y = 1073741824;
result = (2 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 427: RHS constant
x = 2;
result = (x - 1073741824)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 428: both arguments variables
x = 2;
y = 1073741825;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 429: both arguments constants
result = (2 - 1073741825)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 430: LHS constant
y = 1073741825;
result = (2 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 431: RHS constant
x = 2;
result = (x - 1073741825)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 432: both arguments variables
x = 2;
y = -1073741823;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 433: both arguments constants
result = (2 - -1073741823)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 434: LHS constant
y = -1073741823;
result = (2 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 435: RHS constant
x = 2;
result = (x - -1073741823)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 436: both arguments variables
x = 2;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 437: both arguments constants
result = (2 - (-0x3fffffff-1))
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 438: LHS constant
y = (-0x3fffffff-1);
result = (2 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 439: RHS constant
x = 2;
result = (x - (-0x3fffffff-1))
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 440: both arguments variables
x = 2;
y = -1073741825;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 441: both arguments constants
result = (2 - -1073741825)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 442: LHS constant
y = -1073741825;
result = (2 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 443: RHS constant
x = 2;
result = (x - -1073741825)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 444: both arguments variables
x = 2;
y = -1073741826;
result = (x - y);
check = 1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 445: both arguments constants
result = (2 - -1073741826)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 446: LHS constant
y = -1073741826;
result = (2 - y)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 447: RHS constant
x = 2;
result = (x - -1073741826)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 448: both arguments variables
x = 2;
y = 2147483646;
result = (x - y);
check = -2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 449: both arguments constants
result = (2 - 2147483646)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 450: LHS constant
y = 2147483646;
result = (2 - y)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 451: RHS constant
x = 2;
result = (x - 2147483646)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 452: both arguments variables
x = 2;
y = 2147483647;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 453: both arguments constants
result = (2 - 2147483647)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 454: LHS constant
y = 2147483647;
result = (2 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 455: RHS constant
x = 2;
result = (x - 2147483647)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 456: both arguments variables
x = 2;
y = 2147483648;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 457: both arguments constants
result = (2 - 2147483648)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 458: LHS constant
y = 2147483648;
result = (2 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 459: RHS constant
x = 2;
result = (x - 2147483648)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 460: both arguments variables
x = 2;
y = 2147483649;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 461: both arguments constants
result = (2 - 2147483649)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 462: LHS constant
y = 2147483649;
result = (2 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 463: RHS constant
x = 2;
result = (x - 2147483649)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 464: both arguments variables
x = 2;
y = -2147483647;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 465: both arguments constants
result = (2 - -2147483647)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 466: LHS constant
y = -2147483647;
result = (2 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 467: RHS constant
x = 2;
result = (x - -2147483647)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 468: both arguments variables
x = 2;
y = -2147483648;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 469: both arguments constants
result = (2 - -2147483648)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 470: LHS constant
y = -2147483648;
result = (2 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 471: RHS constant
x = 2;
result = (x - -2147483648)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 472: both arguments variables
x = 2;
y = -2147483649;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 473: both arguments constants
result = (2 - -2147483649)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 474: LHS constant
y = -2147483649;
result = (2 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 475: RHS constant
x = 2;
result = (x - -2147483649)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 476: both arguments variables
x = 2;
y = -2147483650;
result = (x - y);
check = 2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 477: both arguments constants
result = (2 - -2147483650)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 478: LHS constant
y = -2147483650;
result = (2 - y)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 479: RHS constant
x = 2;
result = (x - -2147483650)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 480: both arguments variables
x = 2;
y = 4294967295;
result = (x - y);
check = -4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 481: both arguments constants
result = (2 - 4294967295)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 482: LHS constant
y = 4294967295;
result = (2 - y)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 483: RHS constant
x = 2;
result = (x - 4294967295)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 484: both arguments variables
x = 2;
y = 4294967296;
result = (x - y);
check = -4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 485: both arguments constants
result = (2 - 4294967296)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 486: LHS constant
y = 4294967296;
result = (2 - y)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 487: RHS constant
x = 2;
result = (x - 4294967296)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 488: both arguments variables
x = 2;
y = -4294967295;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 489: both arguments constants
result = (2 - -4294967295)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 490: LHS constant
y = -4294967295;
result = (2 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 491: RHS constant
x = 2;
result = (x - -4294967295)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 492: both arguments variables
x = 2;
y = -4294967296;
result = (x - y);
check = 4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 493: both arguments constants
result = (2 - -4294967296)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 494: LHS constant
y = -4294967296;
result = (2 - y)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 495: RHS constant
x = 2;
result = (x - -4294967296)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 496: both arguments variables
x = -2;
y = 0;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 497: both arguments constants
result = (-2 - 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 498: LHS constant
y = 0;
result = (-2 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 499: RHS constant
x = -2;
result = (x - 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test5() {
var x;
var y;
var result;
var check;
// Test 500: both arguments variables
x = -2;
y = 1;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 501: both arguments constants
result = (-2 - 1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 502: LHS constant
y = 1;
result = (-2 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 503: RHS constant
x = -2;
result = (x - 1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 504: both arguments variables
x = -2;
y = -1;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 505: both arguments constants
result = (-2 - -1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 506: LHS constant
y = -1;
result = (-2 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 507: RHS constant
x = -2;
result = (x - -1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 508: both arguments variables
x = -2;
y = 2;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 509: both arguments constants
result = (-2 - 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 510: LHS constant
y = 2;
result = (-2 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 511: RHS constant
x = -2;
result = (x - 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 512: both arguments variables
x = -2;
y = -2;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 513: both arguments constants
result = (-2 - -2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 514: LHS constant
y = -2;
result = (-2 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 515: RHS constant
x = -2;
result = (x - -2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 516: both arguments variables
x = -2;
y = 3;
result = (x - y);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 517: both arguments constants
result = (-2 - 3)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 518: LHS constant
y = 3;
result = (-2 - y)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 519: RHS constant
x = -2;
result = (x - 3)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 520: both arguments variables
x = -2;
y = -3;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 521: both arguments constants
result = (-2 - -3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 522: LHS constant
y = -3;
result = (-2 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 523: RHS constant
x = -2;
result = (x - -3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 524: both arguments variables
x = -2;
y = 4;
result = (x - y);
check = -6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 525: both arguments constants
result = (-2 - 4)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 526: LHS constant
y = 4;
result = (-2 - y)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 527: RHS constant
x = -2;
result = (x - 4)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 528: both arguments variables
x = -2;
y = -4;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 529: both arguments constants
result = (-2 - -4)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 530: LHS constant
y = -4;
result = (-2 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 531: RHS constant
x = -2;
result = (x - -4)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 532: both arguments variables
x = -2;
y = 8;
result = (x - y);
check = -10;
if(result != check) { fail(test, check, result); } ++test; 

// Test 533: both arguments constants
result = (-2 - 8)
check = -10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 534: LHS constant
y = 8;
result = (-2 - y)
check = -10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 535: RHS constant
x = -2;
result = (x - 8)
check = -10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 536: both arguments variables
x = -2;
y = -8;
result = (x - y);
check = 6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 537: both arguments constants
result = (-2 - -8)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 538: LHS constant
y = -8;
result = (-2 - y)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 539: RHS constant
x = -2;
result = (x - -8)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 540: both arguments variables
x = -2;
y = 1073741822;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 541: both arguments constants
result = (-2 - 1073741822)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 542: LHS constant
y = 1073741822;
result = (-2 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 543: RHS constant
x = -2;
result = (x - 1073741822)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 544: both arguments variables
x = -2;
y = 1073741823;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 545: both arguments constants
result = (-2 - 1073741823)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 546: LHS constant
y = 1073741823;
result = (-2 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 547: RHS constant
x = -2;
result = (x - 1073741823)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 548: both arguments variables
x = -2;
y = 1073741824;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 549: both arguments constants
result = (-2 - 1073741824)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 550: LHS constant
y = 1073741824;
result = (-2 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 551: RHS constant
x = -2;
result = (x - 1073741824)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 552: both arguments variables
x = -2;
y = 1073741825;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 553: both arguments constants
result = (-2 - 1073741825)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 554: LHS constant
y = 1073741825;
result = (-2 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 555: RHS constant
x = -2;
result = (x - 1073741825)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 556: both arguments variables
x = -2;
y = -1073741823;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 557: both arguments constants
result = (-2 - -1073741823)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 558: LHS constant
y = -1073741823;
result = (-2 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 559: RHS constant
x = -2;
result = (x - -1073741823)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 560: both arguments variables
x = -2;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 561: both arguments constants
result = (-2 - (-0x3fffffff-1))
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 562: LHS constant
y = (-0x3fffffff-1);
result = (-2 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 563: RHS constant
x = -2;
result = (x - (-0x3fffffff-1))
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 564: both arguments variables
x = -2;
y = -1073741825;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 565: both arguments constants
result = (-2 - -1073741825)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 566: LHS constant
y = -1073741825;
result = (-2 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 567: RHS constant
x = -2;
result = (x - -1073741825)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 568: both arguments variables
x = -2;
y = -1073741826;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 569: both arguments constants
result = (-2 - -1073741826)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 570: LHS constant
y = -1073741826;
result = (-2 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 571: RHS constant
x = -2;
result = (x - -1073741826)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 572: both arguments variables
x = -2;
y = 2147483646;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 573: both arguments constants
result = (-2 - 2147483646)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 574: LHS constant
y = 2147483646;
result = (-2 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 575: RHS constant
x = -2;
result = (x - 2147483646)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 576: both arguments variables
x = -2;
y = 2147483647;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 577: both arguments constants
result = (-2 - 2147483647)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 578: LHS constant
y = 2147483647;
result = (-2 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 579: RHS constant
x = -2;
result = (x - 2147483647)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 580: both arguments variables
x = -2;
y = 2147483648;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 581: both arguments constants
result = (-2 - 2147483648)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 582: LHS constant
y = 2147483648;
result = (-2 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 583: RHS constant
x = -2;
result = (x - 2147483648)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 584: both arguments variables
x = -2;
y = 2147483649;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 585: both arguments constants
result = (-2 - 2147483649)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 586: LHS constant
y = 2147483649;
result = (-2 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 587: RHS constant
x = -2;
result = (x - 2147483649)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 588: both arguments variables
x = -2;
y = -2147483647;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 589: both arguments constants
result = (-2 - -2147483647)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 590: LHS constant
y = -2147483647;
result = (-2 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 591: RHS constant
x = -2;
result = (x - -2147483647)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 592: both arguments variables
x = -2;
y = -2147483648;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 593: both arguments constants
result = (-2 - -2147483648)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 594: LHS constant
y = -2147483648;
result = (-2 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 595: RHS constant
x = -2;
result = (x - -2147483648)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 596: both arguments variables
x = -2;
y = -2147483649;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 597: both arguments constants
result = (-2 - -2147483649)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 598: LHS constant
y = -2147483649;
result = (-2 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 599: RHS constant
x = -2;
result = (x - -2147483649)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test6() {
var x;
var y;
var result;
var check;
// Test 600: both arguments variables
x = -2;
y = -2147483650;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 601: both arguments constants
result = (-2 - -2147483650)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 602: LHS constant
y = -2147483650;
result = (-2 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 603: RHS constant
x = -2;
result = (x - -2147483650)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 604: both arguments variables
x = -2;
y = 4294967295;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 605: both arguments constants
result = (-2 - 4294967295)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 606: LHS constant
y = 4294967295;
result = (-2 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 607: RHS constant
x = -2;
result = (x - 4294967295)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 608: both arguments variables
x = -2;
y = 4294967296;
result = (x - y);
check = -4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 609: both arguments constants
result = (-2 - 4294967296)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 610: LHS constant
y = 4294967296;
result = (-2 - y)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 611: RHS constant
x = -2;
result = (x - 4294967296)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 612: both arguments variables
x = -2;
y = -4294967295;
result = (x - y);
check = 4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 613: both arguments constants
result = (-2 - -4294967295)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 614: LHS constant
y = -4294967295;
result = (-2 - y)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 615: RHS constant
x = -2;
result = (x - -4294967295)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 616: both arguments variables
x = -2;
y = -4294967296;
result = (x - y);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 617: both arguments constants
result = (-2 - -4294967296)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 618: LHS constant
y = -4294967296;
result = (-2 - y)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 619: RHS constant
x = -2;
result = (x - -4294967296)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 620: both arguments variables
x = 3;
y = 0;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 621: both arguments constants
result = (3 - 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 622: LHS constant
y = 0;
result = (3 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 623: RHS constant
x = 3;
result = (x - 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 624: both arguments variables
x = 3;
y = 1;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 625: both arguments constants
result = (3 - 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 626: LHS constant
y = 1;
result = (3 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 627: RHS constant
x = 3;
result = (x - 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 628: both arguments variables
x = 3;
y = -1;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 629: both arguments constants
result = (3 - -1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 630: LHS constant
y = -1;
result = (3 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 631: RHS constant
x = 3;
result = (x - -1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 632: both arguments variables
x = 3;
y = 2;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 633: both arguments constants
result = (3 - 2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 634: LHS constant
y = 2;
result = (3 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 635: RHS constant
x = 3;
result = (x - 2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 636: both arguments variables
x = 3;
y = -2;
result = (x - y);
check = 5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 637: both arguments constants
result = (3 - -2)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 638: LHS constant
y = -2;
result = (3 - y)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 639: RHS constant
x = 3;
result = (x - -2)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 640: both arguments variables
x = 3;
y = 3;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 641: both arguments constants
result = (3 - 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 642: LHS constant
y = 3;
result = (3 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 643: RHS constant
x = 3;
result = (x - 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 644: both arguments variables
x = 3;
y = -3;
result = (x - y);
check = 6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 645: both arguments constants
result = (3 - -3)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 646: LHS constant
y = -3;
result = (3 - y)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 647: RHS constant
x = 3;
result = (x - -3)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 648: both arguments variables
x = 3;
y = 4;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 649: both arguments constants
result = (3 - 4)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 650: LHS constant
y = 4;
result = (3 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 651: RHS constant
x = 3;
result = (x - 4)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 652: both arguments variables
x = 3;
y = -4;
result = (x - y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 653: both arguments constants
result = (3 - -4)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 654: LHS constant
y = -4;
result = (3 - y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 655: RHS constant
x = 3;
result = (x - -4)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 656: both arguments variables
x = 3;
y = 8;
result = (x - y);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 657: both arguments constants
result = (3 - 8)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 658: LHS constant
y = 8;
result = (3 - y)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 659: RHS constant
x = 3;
result = (x - 8)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 660: both arguments variables
x = 3;
y = -8;
result = (x - y);
check = 11;
if(result != check) { fail(test, check, result); } ++test; 

// Test 661: both arguments constants
result = (3 - -8)
check = 11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 662: LHS constant
y = -8;
result = (3 - y)
check = 11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 663: RHS constant
x = 3;
result = (x - -8)
check = 11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 664: both arguments variables
x = 3;
y = 1073741822;
result = (x - y);
check = -1073741819;
if(result != check) { fail(test, check, result); } ++test; 

// Test 665: both arguments constants
result = (3 - 1073741822)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 666: LHS constant
y = 1073741822;
result = (3 - y)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 667: RHS constant
x = 3;
result = (x - 1073741822)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 668: both arguments variables
x = 3;
y = 1073741823;
result = (x - y);
check = -1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 669: both arguments constants
result = (3 - 1073741823)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 670: LHS constant
y = 1073741823;
result = (3 - y)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 671: RHS constant
x = 3;
result = (x - 1073741823)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 672: both arguments variables
x = 3;
y = 1073741824;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 673: both arguments constants
result = (3 - 1073741824)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 674: LHS constant
y = 1073741824;
result = (3 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 675: RHS constant
x = 3;
result = (x - 1073741824)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 676: both arguments variables
x = 3;
y = 1073741825;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 677: both arguments constants
result = (3 - 1073741825)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 678: LHS constant
y = 1073741825;
result = (3 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 679: RHS constant
x = 3;
result = (x - 1073741825)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 680: both arguments variables
x = 3;
y = -1073741823;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 681: both arguments constants
result = (3 - -1073741823)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 682: LHS constant
y = -1073741823;
result = (3 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 683: RHS constant
x = 3;
result = (x - -1073741823)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 684: both arguments variables
x = 3;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 685: both arguments constants
result = (3 - (-0x3fffffff-1))
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 686: LHS constant
y = (-0x3fffffff-1);
result = (3 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 687: RHS constant
x = 3;
result = (x - (-0x3fffffff-1))
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 688: both arguments variables
x = 3;
y = -1073741825;
result = (x - y);
check = 1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 689: both arguments constants
result = (3 - -1073741825)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 690: LHS constant
y = -1073741825;
result = (3 - y)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 691: RHS constant
x = 3;
result = (x - -1073741825)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 692: both arguments variables
x = 3;
y = -1073741826;
result = (x - y);
check = 1073741829;
if(result != check) { fail(test, check, result); } ++test; 

// Test 693: both arguments constants
result = (3 - -1073741826)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 694: LHS constant
y = -1073741826;
result = (3 - y)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 695: RHS constant
x = 3;
result = (x - -1073741826)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 696: both arguments variables
x = 3;
y = 2147483646;
result = (x - y);
check = -2147483643;
if(result != check) { fail(test, check, result); } ++test; 

// Test 697: both arguments constants
result = (3 - 2147483646)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 698: LHS constant
y = 2147483646;
result = (3 - y)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 699: RHS constant
x = 3;
result = (x - 2147483646)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test7() {
var x;
var y;
var result;
var check;
// Test 700: both arguments variables
x = 3;
y = 2147483647;
result = (x - y);
check = -2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 701: both arguments constants
result = (3 - 2147483647)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 702: LHS constant
y = 2147483647;
result = (3 - y)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 703: RHS constant
x = 3;
result = (x - 2147483647)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 704: both arguments variables
x = 3;
y = 2147483648;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 705: both arguments constants
result = (3 - 2147483648)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 706: LHS constant
y = 2147483648;
result = (3 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 707: RHS constant
x = 3;
result = (x - 2147483648)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 708: both arguments variables
x = 3;
y = 2147483649;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 709: both arguments constants
result = (3 - 2147483649)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 710: LHS constant
y = 2147483649;
result = (3 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 711: RHS constant
x = 3;
result = (x - 2147483649)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 712: both arguments variables
x = 3;
y = -2147483647;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 713: both arguments constants
result = (3 - -2147483647)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 714: LHS constant
y = -2147483647;
result = (3 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 715: RHS constant
x = 3;
result = (x - -2147483647)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 716: both arguments variables
x = 3;
y = -2147483648;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 717: both arguments constants
result = (3 - -2147483648)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 718: LHS constant
y = -2147483648;
result = (3 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 719: RHS constant
x = 3;
result = (x - -2147483648)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 720: both arguments variables
x = 3;
y = -2147483649;
result = (x - y);
check = 2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 721: both arguments constants
result = (3 - -2147483649)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 722: LHS constant
y = -2147483649;
result = (3 - y)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 723: RHS constant
x = 3;
result = (x - -2147483649)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 724: both arguments variables
x = 3;
y = -2147483650;
result = (x - y);
check = 2147483653;
if(result != check) { fail(test, check, result); } ++test; 

// Test 725: both arguments constants
result = (3 - -2147483650)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 726: LHS constant
y = -2147483650;
result = (3 - y)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 727: RHS constant
x = 3;
result = (x - -2147483650)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 728: both arguments variables
x = 3;
y = 4294967295;
result = (x - y);
check = -4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 729: both arguments constants
result = (3 - 4294967295)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 730: LHS constant
y = 4294967295;
result = (3 - y)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 731: RHS constant
x = 3;
result = (x - 4294967295)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 732: both arguments variables
x = 3;
y = 4294967296;
result = (x - y);
check = -4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 733: both arguments constants
result = (3 - 4294967296)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 734: LHS constant
y = 4294967296;
result = (3 - y)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 735: RHS constant
x = 3;
result = (x - 4294967296)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 736: both arguments variables
x = 3;
y = -4294967295;
result = (x - y);
check = 4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 737: both arguments constants
result = (3 - -4294967295)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 738: LHS constant
y = -4294967295;
result = (3 - y)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 739: RHS constant
x = 3;
result = (x - -4294967295)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 740: both arguments variables
x = 3;
y = -4294967296;
result = (x - y);
check = 4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 741: both arguments constants
result = (3 - -4294967296)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 742: LHS constant
y = -4294967296;
result = (3 - y)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 743: RHS constant
x = 3;
result = (x - -4294967296)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 744: both arguments variables
x = -3;
y = 0;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 745: both arguments constants
result = (-3 - 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 746: LHS constant
y = 0;
result = (-3 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 747: RHS constant
x = -3;
result = (x - 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 748: both arguments variables
x = -3;
y = 1;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 749: both arguments constants
result = (-3 - 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 750: LHS constant
y = 1;
result = (-3 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 751: RHS constant
x = -3;
result = (x - 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 752: both arguments variables
x = -3;
y = -1;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 753: both arguments constants
result = (-3 - -1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 754: LHS constant
y = -1;
result = (-3 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 755: RHS constant
x = -3;
result = (x - -1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 756: both arguments variables
x = -3;
y = 2;
result = (x - y);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 757: both arguments constants
result = (-3 - 2)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 758: LHS constant
y = 2;
result = (-3 - y)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 759: RHS constant
x = -3;
result = (x - 2)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 760: both arguments variables
x = -3;
y = -2;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 761: both arguments constants
result = (-3 - -2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 762: LHS constant
y = -2;
result = (-3 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 763: RHS constant
x = -3;
result = (x - -2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 764: both arguments variables
x = -3;
y = 3;
result = (x - y);
check = -6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 765: both arguments constants
result = (-3 - 3)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 766: LHS constant
y = 3;
result = (-3 - y)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 767: RHS constant
x = -3;
result = (x - 3)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 768: both arguments variables
x = -3;
y = -3;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 769: both arguments constants
result = (-3 - -3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 770: LHS constant
y = -3;
result = (-3 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 771: RHS constant
x = -3;
result = (x - -3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 772: both arguments variables
x = -3;
y = 4;
result = (x - y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 773: both arguments constants
result = (-3 - 4)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 774: LHS constant
y = 4;
result = (-3 - y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 775: RHS constant
x = -3;
result = (x - 4)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 776: both arguments variables
x = -3;
y = -4;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 777: both arguments constants
result = (-3 - -4)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 778: LHS constant
y = -4;
result = (-3 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 779: RHS constant
x = -3;
result = (x - -4)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 780: both arguments variables
x = -3;
y = 8;
result = (x - y);
check = -11;
if(result != check) { fail(test, check, result); } ++test; 

// Test 781: both arguments constants
result = (-3 - 8)
check = -11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 782: LHS constant
y = 8;
result = (-3 - y)
check = -11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 783: RHS constant
x = -3;
result = (x - 8)
check = -11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 784: both arguments variables
x = -3;
y = -8;
result = (x - y);
check = 5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 785: both arguments constants
result = (-3 - -8)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 786: LHS constant
y = -8;
result = (-3 - y)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 787: RHS constant
x = -3;
result = (x - -8)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 788: both arguments variables
x = -3;
y = 1073741822;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 789: both arguments constants
result = (-3 - 1073741822)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 790: LHS constant
y = 1073741822;
result = (-3 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 791: RHS constant
x = -3;
result = (x - 1073741822)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 792: both arguments variables
x = -3;
y = 1073741823;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 793: both arguments constants
result = (-3 - 1073741823)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 794: LHS constant
y = 1073741823;
result = (-3 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 795: RHS constant
x = -3;
result = (x - 1073741823)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 796: both arguments variables
x = -3;
y = 1073741824;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 797: both arguments constants
result = (-3 - 1073741824)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 798: LHS constant
y = 1073741824;
result = (-3 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 799: RHS constant
x = -3;
result = (x - 1073741824)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test8() {
var x;
var y;
var result;
var check;
// Test 800: both arguments variables
x = -3;
y = 1073741825;
result = (x - y);
check = -1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 801: both arguments constants
result = (-3 - 1073741825)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 802: LHS constant
y = 1073741825;
result = (-3 - y)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 803: RHS constant
x = -3;
result = (x - 1073741825)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 804: both arguments variables
x = -3;
y = -1073741823;
result = (x - y);
check = 1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 805: both arguments constants
result = (-3 - -1073741823)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 806: LHS constant
y = -1073741823;
result = (-3 - y)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 807: RHS constant
x = -3;
result = (x - -1073741823)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 808: both arguments variables
x = -3;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 809: both arguments constants
result = (-3 - (-0x3fffffff-1))
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 810: LHS constant
y = (-0x3fffffff-1);
result = (-3 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 811: RHS constant
x = -3;
result = (x - (-0x3fffffff-1))
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 812: both arguments variables
x = -3;
y = -1073741825;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 813: both arguments constants
result = (-3 - -1073741825)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 814: LHS constant
y = -1073741825;
result = (-3 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 815: RHS constant
x = -3;
result = (x - -1073741825)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 816: both arguments variables
x = -3;
y = -1073741826;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 817: both arguments constants
result = (-3 - -1073741826)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 818: LHS constant
y = -1073741826;
result = (-3 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 819: RHS constant
x = -3;
result = (x - -1073741826)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 820: both arguments variables
x = -3;
y = 2147483646;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 821: both arguments constants
result = (-3 - 2147483646)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 822: LHS constant
y = 2147483646;
result = (-3 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 823: RHS constant
x = -3;
result = (x - 2147483646)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 824: both arguments variables
x = -3;
y = 2147483647;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 825: both arguments constants
result = (-3 - 2147483647)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 826: LHS constant
y = 2147483647;
result = (-3 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 827: RHS constant
x = -3;
result = (x - 2147483647)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 828: both arguments variables
x = -3;
y = 2147483648;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 829: both arguments constants
result = (-3 - 2147483648)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 830: LHS constant
y = 2147483648;
result = (-3 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 831: RHS constant
x = -3;
result = (x - 2147483648)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 832: both arguments variables
x = -3;
y = 2147483649;
result = (x - y);
check = -2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 833: both arguments constants
result = (-3 - 2147483649)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 834: LHS constant
y = 2147483649;
result = (-3 - y)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 835: RHS constant
x = -3;
result = (x - 2147483649)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 836: both arguments variables
x = -3;
y = -2147483647;
result = (x - y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 837: both arguments constants
result = (-3 - -2147483647)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 838: LHS constant
y = -2147483647;
result = (-3 - y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 839: RHS constant
x = -3;
result = (x - -2147483647)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 840: both arguments variables
x = -3;
y = -2147483648;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 841: both arguments constants
result = (-3 - -2147483648)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 842: LHS constant
y = -2147483648;
result = (-3 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 843: RHS constant
x = -3;
result = (x - -2147483648)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 844: both arguments variables
x = -3;
y = -2147483649;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 845: both arguments constants
result = (-3 - -2147483649)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 846: LHS constant
y = -2147483649;
result = (-3 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 847: RHS constant
x = -3;
result = (x - -2147483649)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 848: both arguments variables
x = -3;
y = -2147483650;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 849: both arguments constants
result = (-3 - -2147483650)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 850: LHS constant
y = -2147483650;
result = (-3 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 851: RHS constant
x = -3;
result = (x - -2147483650)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 852: both arguments variables
x = -3;
y = 4294967295;
result = (x - y);
check = -4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 853: both arguments constants
result = (-3 - 4294967295)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 854: LHS constant
y = 4294967295;
result = (-3 - y)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 855: RHS constant
x = -3;
result = (x - 4294967295)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 856: both arguments variables
x = -3;
y = 4294967296;
result = (x - y);
check = -4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 857: both arguments constants
result = (-3 - 4294967296)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 858: LHS constant
y = 4294967296;
result = (-3 - y)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 859: RHS constant
x = -3;
result = (x - 4294967296)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 860: both arguments variables
x = -3;
y = -4294967295;
result = (x - y);
check = 4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 861: both arguments constants
result = (-3 - -4294967295)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 862: LHS constant
y = -4294967295;
result = (-3 - y)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 863: RHS constant
x = -3;
result = (x - -4294967295)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 864: both arguments variables
x = -3;
y = -4294967296;
result = (x - y);
check = 4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 865: both arguments constants
result = (-3 - -4294967296)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 866: LHS constant
y = -4294967296;
result = (-3 - y)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 867: RHS constant
x = -3;
result = (x - -4294967296)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 868: both arguments variables
x = 4;
y = 0;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 869: both arguments constants
result = (4 - 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 870: LHS constant
y = 0;
result = (4 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 871: RHS constant
x = 4;
result = (x - 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 872: both arguments variables
x = 4;
y = 1;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 873: both arguments constants
result = (4 - 1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 874: LHS constant
y = 1;
result = (4 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 875: RHS constant
x = 4;
result = (x - 1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 876: both arguments variables
x = 4;
y = -1;
result = (x - y);
check = 5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 877: both arguments constants
result = (4 - -1)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 878: LHS constant
y = -1;
result = (4 - y)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 879: RHS constant
x = 4;
result = (x - -1)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 880: both arguments variables
x = 4;
y = 2;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 881: both arguments constants
result = (4 - 2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 882: LHS constant
y = 2;
result = (4 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 883: RHS constant
x = 4;
result = (x - 2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 884: both arguments variables
x = 4;
y = -2;
result = (x - y);
check = 6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 885: both arguments constants
result = (4 - -2)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 886: LHS constant
y = -2;
result = (4 - y)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 887: RHS constant
x = 4;
result = (x - -2)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 888: both arguments variables
x = 4;
y = 3;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 889: both arguments constants
result = (4 - 3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 890: LHS constant
y = 3;
result = (4 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 891: RHS constant
x = 4;
result = (x - 3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 892: both arguments variables
x = 4;
y = -3;
result = (x - y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 893: both arguments constants
result = (4 - -3)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 894: LHS constant
y = -3;
result = (4 - y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 895: RHS constant
x = 4;
result = (x - -3)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 896: both arguments variables
x = 4;
y = 4;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 897: both arguments constants
result = (4 - 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 898: LHS constant
y = 4;
result = (4 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 899: RHS constant
x = 4;
result = (x - 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test9() {
var x;
var y;
var result;
var check;
// Test 900: both arguments variables
x = 4;
y = -4;
result = (x - y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 901: both arguments constants
result = (4 - -4)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 902: LHS constant
y = -4;
result = (4 - y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 903: RHS constant
x = 4;
result = (x - -4)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 904: both arguments variables
x = 4;
y = 8;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 905: both arguments constants
result = (4 - 8)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 906: LHS constant
y = 8;
result = (4 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 907: RHS constant
x = 4;
result = (x - 8)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 908: both arguments variables
x = 4;
y = -8;
result = (x - y);
check = 12;
if(result != check) { fail(test, check, result); } ++test; 

// Test 909: both arguments constants
result = (4 - -8)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 910: LHS constant
y = -8;
result = (4 - y)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 911: RHS constant
x = 4;
result = (x - -8)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 912: both arguments variables
x = 4;
y = 1073741822;
result = (x - y);
check = -1073741818;
if(result != check) { fail(test, check, result); } ++test; 

// Test 913: both arguments constants
result = (4 - 1073741822)
check = -1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 914: LHS constant
y = 1073741822;
result = (4 - y)
check = -1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 915: RHS constant
x = 4;
result = (x - 1073741822)
check = -1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 916: both arguments variables
x = 4;
y = 1073741823;
result = (x - y);
check = -1073741819;
if(result != check) { fail(test, check, result); } ++test; 

// Test 917: both arguments constants
result = (4 - 1073741823)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 918: LHS constant
y = 1073741823;
result = (4 - y)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 919: RHS constant
x = 4;
result = (x - 1073741823)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 920: both arguments variables
x = 4;
y = 1073741824;
result = (x - y);
check = -1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 921: both arguments constants
result = (4 - 1073741824)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 922: LHS constant
y = 1073741824;
result = (4 - y)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 923: RHS constant
x = 4;
result = (x - 1073741824)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 924: both arguments variables
x = 4;
y = 1073741825;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 925: both arguments constants
result = (4 - 1073741825)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 926: LHS constant
y = 1073741825;
result = (4 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 927: RHS constant
x = 4;
result = (x - 1073741825)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 928: both arguments variables
x = 4;
y = -1073741823;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 929: both arguments constants
result = (4 - -1073741823)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 930: LHS constant
y = -1073741823;
result = (4 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 931: RHS constant
x = 4;
result = (x - -1073741823)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 932: both arguments variables
x = 4;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 933: both arguments constants
result = (4 - (-0x3fffffff-1))
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 934: LHS constant
y = (-0x3fffffff-1);
result = (4 - y)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 935: RHS constant
x = 4;
result = (x - (-0x3fffffff-1))
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 936: both arguments variables
x = 4;
y = -1073741825;
result = (x - y);
check = 1073741829;
if(result != check) { fail(test, check, result); } ++test; 

// Test 937: both arguments constants
result = (4 - -1073741825)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 938: LHS constant
y = -1073741825;
result = (4 - y)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 939: RHS constant
x = 4;
result = (x - -1073741825)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 940: both arguments variables
x = 4;
y = -1073741826;
result = (x - y);
check = 1073741830;
if(result != check) { fail(test, check, result); } ++test; 

// Test 941: both arguments constants
result = (4 - -1073741826)
check = 1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 942: LHS constant
y = -1073741826;
result = (4 - y)
check = 1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 943: RHS constant
x = 4;
result = (x - -1073741826)
check = 1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 944: both arguments variables
x = 4;
y = 2147483646;
result = (x - y);
check = -2147483642;
if(result != check) { fail(test, check, result); } ++test; 

// Test 945: both arguments constants
result = (4 - 2147483646)
check = -2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 946: LHS constant
y = 2147483646;
result = (4 - y)
check = -2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 947: RHS constant
x = 4;
result = (x - 2147483646)
check = -2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 948: both arguments variables
x = 4;
y = 2147483647;
result = (x - y);
check = -2147483643;
if(result != check) { fail(test, check, result); } ++test; 

// Test 949: both arguments constants
result = (4 - 2147483647)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 950: LHS constant
y = 2147483647;
result = (4 - y)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 951: RHS constant
x = 4;
result = (x - 2147483647)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 952: both arguments variables
x = 4;
y = 2147483648;
result = (x - y);
check = -2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 953: both arguments constants
result = (4 - 2147483648)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 954: LHS constant
y = 2147483648;
result = (4 - y)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 955: RHS constant
x = 4;
result = (x - 2147483648)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 956: both arguments variables
x = 4;
y = 2147483649;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 957: both arguments constants
result = (4 - 2147483649)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 958: LHS constant
y = 2147483649;
result = (4 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 959: RHS constant
x = 4;
result = (x - 2147483649)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 960: both arguments variables
x = 4;
y = -2147483647;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 961: both arguments constants
result = (4 - -2147483647)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 962: LHS constant
y = -2147483647;
result = (4 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 963: RHS constant
x = 4;
result = (x - -2147483647)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 964: both arguments variables
x = 4;
y = -2147483648;
result = (x - y);
check = 2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 965: both arguments constants
result = (4 - -2147483648)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 966: LHS constant
y = -2147483648;
result = (4 - y)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 967: RHS constant
x = 4;
result = (x - -2147483648)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 968: both arguments variables
x = 4;
y = -2147483649;
result = (x - y);
check = 2147483653;
if(result != check) { fail(test, check, result); } ++test; 

// Test 969: both arguments constants
result = (4 - -2147483649)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 970: LHS constant
y = -2147483649;
result = (4 - y)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 971: RHS constant
x = 4;
result = (x - -2147483649)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 972: both arguments variables
x = 4;
y = -2147483650;
result = (x - y);
check = 2147483654;
if(result != check) { fail(test, check, result); } ++test; 

// Test 973: both arguments constants
result = (4 - -2147483650)
check = 2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 974: LHS constant
y = -2147483650;
result = (4 - y)
check = 2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 975: RHS constant
x = 4;
result = (x - -2147483650)
check = 2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 976: both arguments variables
x = 4;
y = 4294967295;
result = (x - y);
check = -4294967291;
if(result != check) { fail(test, check, result); } ++test; 

// Test 977: both arguments constants
result = (4 - 4294967295)
check = -4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 978: LHS constant
y = 4294967295;
result = (4 - y)
check = -4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 979: RHS constant
x = 4;
result = (x - 4294967295)
check = -4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 980: both arguments variables
x = 4;
y = 4294967296;
result = (x - y);
check = -4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 981: both arguments constants
result = (4 - 4294967296)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 982: LHS constant
y = 4294967296;
result = (4 - y)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 983: RHS constant
x = 4;
result = (x - 4294967296)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 984: both arguments variables
x = 4;
y = -4294967295;
result = (x - y);
check = 4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 985: both arguments constants
result = (4 - -4294967295)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 986: LHS constant
y = -4294967295;
result = (4 - y)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 987: RHS constant
x = 4;
result = (x - -4294967295)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 988: both arguments variables
x = 4;
y = -4294967296;
result = (x - y);
check = 4294967300;
if(result != check) { fail(test, check, result); } ++test; 

// Test 989: both arguments constants
result = (4 - -4294967296)
check = 4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 990: LHS constant
y = -4294967296;
result = (4 - y)
check = 4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 991: RHS constant
x = 4;
result = (x - -4294967296)
check = 4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 992: both arguments variables
x = -4;
y = 0;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 993: both arguments constants
result = (-4 - 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 994: LHS constant
y = 0;
result = (-4 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 995: RHS constant
x = -4;
result = (x - 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 996: both arguments variables
x = -4;
y = 1;
result = (x - y);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 997: both arguments constants
result = (-4 - 1)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 998: LHS constant
y = 1;
result = (-4 - y)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 999: RHS constant
x = -4;
result = (x - 1)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test10() {
var x;
var y;
var result;
var check;
// Test 1000: both arguments variables
x = -4;
y = -1;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1001: both arguments constants
result = (-4 - -1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1002: LHS constant
y = -1;
result = (-4 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1003: RHS constant
x = -4;
result = (x - -1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1004: both arguments variables
x = -4;
y = 2;
result = (x - y);
check = -6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1005: both arguments constants
result = (-4 - 2)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1006: LHS constant
y = 2;
result = (-4 - y)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1007: RHS constant
x = -4;
result = (x - 2)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1008: both arguments variables
x = -4;
y = -2;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1009: both arguments constants
result = (-4 - -2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1010: LHS constant
y = -2;
result = (-4 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1011: RHS constant
x = -4;
result = (x - -2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1012: both arguments variables
x = -4;
y = 3;
result = (x - y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1013: both arguments constants
result = (-4 - 3)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1014: LHS constant
y = 3;
result = (-4 - y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1015: RHS constant
x = -4;
result = (x - 3)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1016: both arguments variables
x = -4;
y = -3;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1017: both arguments constants
result = (-4 - -3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1018: LHS constant
y = -3;
result = (-4 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1019: RHS constant
x = -4;
result = (x - -3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1020: both arguments variables
x = -4;
y = 4;
result = (x - y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1021: both arguments constants
result = (-4 - 4)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1022: LHS constant
y = 4;
result = (-4 - y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1023: RHS constant
x = -4;
result = (x - 4)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1024: both arguments variables
x = -4;
y = -4;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1025: both arguments constants
result = (-4 - -4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1026: LHS constant
y = -4;
result = (-4 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1027: RHS constant
x = -4;
result = (x - -4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1028: both arguments variables
x = -4;
y = 8;
result = (x - y);
check = -12;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1029: both arguments constants
result = (-4 - 8)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1030: LHS constant
y = 8;
result = (-4 - y)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1031: RHS constant
x = -4;
result = (x - 8)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1032: both arguments variables
x = -4;
y = -8;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1033: both arguments constants
result = (-4 - -8)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1034: LHS constant
y = -8;
result = (-4 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1035: RHS constant
x = -4;
result = (x - -8)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1036: both arguments variables
x = -4;
y = 1073741822;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1037: both arguments constants
result = (-4 - 1073741822)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1038: LHS constant
y = 1073741822;
result = (-4 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1039: RHS constant
x = -4;
result = (x - 1073741822)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1040: both arguments variables
x = -4;
y = 1073741823;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1041: both arguments constants
result = (-4 - 1073741823)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1042: LHS constant
y = 1073741823;
result = (-4 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1043: RHS constant
x = -4;
result = (x - 1073741823)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1044: both arguments variables
x = -4;
y = 1073741824;
result = (x - y);
check = -1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1045: both arguments constants
result = (-4 - 1073741824)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1046: LHS constant
y = 1073741824;
result = (-4 - y)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1047: RHS constant
x = -4;
result = (x - 1073741824)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1048: both arguments variables
x = -4;
y = 1073741825;
result = (x - y);
check = -1073741829;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1049: both arguments constants
result = (-4 - 1073741825)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1050: LHS constant
y = 1073741825;
result = (-4 - y)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1051: RHS constant
x = -4;
result = (x - 1073741825)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1052: both arguments variables
x = -4;
y = -1073741823;
result = (x - y);
check = 1073741819;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1053: both arguments constants
result = (-4 - -1073741823)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1054: LHS constant
y = -1073741823;
result = (-4 - y)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1055: RHS constant
x = -4;
result = (x - -1073741823)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1056: both arguments variables
x = -4;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1057: both arguments constants
result = (-4 - (-0x3fffffff-1))
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1058: LHS constant
y = (-0x3fffffff-1);
result = (-4 - y)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1059: RHS constant
x = -4;
result = (x - (-0x3fffffff-1))
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1060: both arguments variables
x = -4;
y = -1073741825;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1061: both arguments constants
result = (-4 - -1073741825)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1062: LHS constant
y = -1073741825;
result = (-4 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1063: RHS constant
x = -4;
result = (x - -1073741825)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1064: both arguments variables
x = -4;
y = -1073741826;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1065: both arguments constants
result = (-4 - -1073741826)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1066: LHS constant
y = -1073741826;
result = (-4 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1067: RHS constant
x = -4;
result = (x - -1073741826)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1068: both arguments variables
x = -4;
y = 2147483646;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1069: both arguments constants
result = (-4 - 2147483646)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1070: LHS constant
y = 2147483646;
result = (-4 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1071: RHS constant
x = -4;
result = (x - 2147483646)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1072: both arguments variables
x = -4;
y = 2147483647;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1073: both arguments constants
result = (-4 - 2147483647)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1074: LHS constant
y = 2147483647;
result = (-4 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1075: RHS constant
x = -4;
result = (x - 2147483647)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1076: both arguments variables
x = -4;
y = 2147483648;
result = (x - y);
check = -2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1077: both arguments constants
result = (-4 - 2147483648)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1078: LHS constant
y = 2147483648;
result = (-4 - y)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1079: RHS constant
x = -4;
result = (x - 2147483648)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1080: both arguments variables
x = -4;
y = 2147483649;
result = (x - y);
check = -2147483653;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1081: both arguments constants
result = (-4 - 2147483649)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1082: LHS constant
y = 2147483649;
result = (-4 - y)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1083: RHS constant
x = -4;
result = (x - 2147483649)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1084: both arguments variables
x = -4;
y = -2147483647;
result = (x - y);
check = 2147483643;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1085: both arguments constants
result = (-4 - -2147483647)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1086: LHS constant
y = -2147483647;
result = (-4 - y)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1087: RHS constant
x = -4;
result = (x - -2147483647)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1088: both arguments variables
x = -4;
y = -2147483648;
result = (x - y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1089: both arguments constants
result = (-4 - -2147483648)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1090: LHS constant
y = -2147483648;
result = (-4 - y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1091: RHS constant
x = -4;
result = (x - -2147483648)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1092: both arguments variables
x = -4;
y = -2147483649;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1093: both arguments constants
result = (-4 - -2147483649)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1094: LHS constant
y = -2147483649;
result = (-4 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1095: RHS constant
x = -4;
result = (x - -2147483649)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1096: both arguments variables
x = -4;
y = -2147483650;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1097: both arguments constants
result = (-4 - -2147483650)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1098: LHS constant
y = -2147483650;
result = (-4 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1099: RHS constant
x = -4;
result = (x - -2147483650)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test11() {
var x;
var y;
var result;
var check;
// Test 1100: both arguments variables
x = -4;
y = 4294967295;
result = (x - y);
check = -4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1101: both arguments constants
result = (-4 - 4294967295)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1102: LHS constant
y = 4294967295;
result = (-4 - y)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1103: RHS constant
x = -4;
result = (x - 4294967295)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1104: both arguments variables
x = -4;
y = 4294967296;
result = (x - y);
check = -4294967300;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1105: both arguments constants
result = (-4 - 4294967296)
check = -4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1106: LHS constant
y = 4294967296;
result = (-4 - y)
check = -4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1107: RHS constant
x = -4;
result = (x - 4294967296)
check = -4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1108: both arguments variables
x = -4;
y = -4294967295;
result = (x - y);
check = 4294967291;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1109: both arguments constants
result = (-4 - -4294967295)
check = 4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1110: LHS constant
y = -4294967295;
result = (-4 - y)
check = 4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1111: RHS constant
x = -4;
result = (x - -4294967295)
check = 4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1112: both arguments variables
x = -4;
y = -4294967296;
result = (x - y);
check = 4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1113: both arguments constants
result = (-4 - -4294967296)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1114: LHS constant
y = -4294967296;
result = (-4 - y)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1115: RHS constant
x = -4;
result = (x - -4294967296)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1116: both arguments variables
x = 8;
y = 0;
result = (x - y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1117: both arguments constants
result = (8 - 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1118: LHS constant
y = 0;
result = (8 - y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1119: RHS constant
x = 8;
result = (x - 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1120: both arguments variables
x = 8;
y = 1;
result = (x - y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1121: both arguments constants
result = (8 - 1)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1122: LHS constant
y = 1;
result = (8 - y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1123: RHS constant
x = 8;
result = (x - 1)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1124: both arguments variables
x = 8;
y = -1;
result = (x - y);
check = 9;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1125: both arguments constants
result = (8 - -1)
check = 9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1126: LHS constant
y = -1;
result = (8 - y)
check = 9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1127: RHS constant
x = 8;
result = (x - -1)
check = 9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1128: both arguments variables
x = 8;
y = 2;
result = (x - y);
check = 6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1129: both arguments constants
result = (8 - 2)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1130: LHS constant
y = 2;
result = (8 - y)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1131: RHS constant
x = 8;
result = (x - 2)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1132: both arguments variables
x = 8;
y = -2;
result = (x - y);
check = 10;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1133: both arguments constants
result = (8 - -2)
check = 10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1134: LHS constant
y = -2;
result = (8 - y)
check = 10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1135: RHS constant
x = 8;
result = (x - -2)
check = 10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1136: both arguments variables
x = 8;
y = 3;
result = (x - y);
check = 5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1137: both arguments constants
result = (8 - 3)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1138: LHS constant
y = 3;
result = (8 - y)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1139: RHS constant
x = 8;
result = (x - 3)
check = 5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1140: both arguments variables
x = 8;
y = -3;
result = (x - y);
check = 11;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1141: both arguments constants
result = (8 - -3)
check = 11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1142: LHS constant
y = -3;
result = (8 - y)
check = 11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1143: RHS constant
x = 8;
result = (x - -3)
check = 11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1144: both arguments variables
x = 8;
y = 4;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1145: both arguments constants
result = (8 - 4)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1146: LHS constant
y = 4;
result = (8 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1147: RHS constant
x = 8;
result = (x - 4)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1148: both arguments variables
x = 8;
y = -4;
result = (x - y);
check = 12;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1149: both arguments constants
result = (8 - -4)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1150: LHS constant
y = -4;
result = (8 - y)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1151: RHS constant
x = 8;
result = (x - -4)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1152: both arguments variables
x = 8;
y = 8;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1153: both arguments constants
result = (8 - 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1154: LHS constant
y = 8;
result = (8 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1155: RHS constant
x = 8;
result = (x - 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1156: both arguments variables
x = 8;
y = -8;
result = (x - y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1157: both arguments constants
result = (8 - -8)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1158: LHS constant
y = -8;
result = (8 - y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1159: RHS constant
x = 8;
result = (x - -8)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1160: both arguments variables
x = 8;
y = 1073741822;
result = (x - y);
check = -1073741814;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1161: both arguments constants
result = (8 - 1073741822)
check = -1073741814
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1162: LHS constant
y = 1073741822;
result = (8 - y)
check = -1073741814
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1163: RHS constant
x = 8;
result = (x - 1073741822)
check = -1073741814
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1164: both arguments variables
x = 8;
y = 1073741823;
result = (x - y);
check = -1073741815;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1165: both arguments constants
result = (8 - 1073741823)
check = -1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1166: LHS constant
y = 1073741823;
result = (8 - y)
check = -1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1167: RHS constant
x = 8;
result = (x - 1073741823)
check = -1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1168: both arguments variables
x = 8;
y = 1073741824;
result = (x - y);
check = -1073741816;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1169: both arguments constants
result = (8 - 1073741824)
check = -1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1170: LHS constant
y = 1073741824;
result = (8 - y)
check = -1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1171: RHS constant
x = 8;
result = (x - 1073741824)
check = -1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1172: both arguments variables
x = 8;
y = 1073741825;
result = (x - y);
check = -1073741817;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1173: both arguments constants
result = (8 - 1073741825)
check = -1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1174: LHS constant
y = 1073741825;
result = (8 - y)
check = -1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1175: RHS constant
x = 8;
result = (x - 1073741825)
check = -1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1176: both arguments variables
x = 8;
y = -1073741823;
result = (x - y);
check = 1073741831;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1177: both arguments constants
result = (8 - -1073741823)
check = 1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1178: LHS constant
y = -1073741823;
result = (8 - y)
check = 1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1179: RHS constant
x = 8;
result = (x - -1073741823)
check = 1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1180: both arguments variables
x = 8;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741832;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1181: both arguments constants
result = (8 - (-0x3fffffff-1))
check = 1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1182: LHS constant
y = (-0x3fffffff-1);
result = (8 - y)
check = 1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1183: RHS constant
x = 8;
result = (x - (-0x3fffffff-1))
check = 1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1184: both arguments variables
x = 8;
y = -1073741825;
result = (x - y);
check = 1073741833;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1185: both arguments constants
result = (8 - -1073741825)
check = 1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1186: LHS constant
y = -1073741825;
result = (8 - y)
check = 1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1187: RHS constant
x = 8;
result = (x - -1073741825)
check = 1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1188: both arguments variables
x = 8;
y = -1073741826;
result = (x - y);
check = 1073741834;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1189: both arguments constants
result = (8 - -1073741826)
check = 1073741834
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1190: LHS constant
y = -1073741826;
result = (8 - y)
check = 1073741834
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1191: RHS constant
x = 8;
result = (x - -1073741826)
check = 1073741834
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1192: both arguments variables
x = 8;
y = 2147483646;
result = (x - y);
check = -2147483638;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1193: both arguments constants
result = (8 - 2147483646)
check = -2147483638
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1194: LHS constant
y = 2147483646;
result = (8 - y)
check = -2147483638
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1195: RHS constant
x = 8;
result = (x - 2147483646)
check = -2147483638
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1196: both arguments variables
x = 8;
y = 2147483647;
result = (x - y);
check = -2147483639;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1197: both arguments constants
result = (8 - 2147483647)
check = -2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1198: LHS constant
y = 2147483647;
result = (8 - y)
check = -2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1199: RHS constant
x = 8;
result = (x - 2147483647)
check = -2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test12() {
var x;
var y;
var result;
var check;
// Test 1200: both arguments variables
x = 8;
y = 2147483648;
result = (x - y);
check = -2147483640;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1201: both arguments constants
result = (8 - 2147483648)
check = -2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1202: LHS constant
y = 2147483648;
result = (8 - y)
check = -2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1203: RHS constant
x = 8;
result = (x - 2147483648)
check = -2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1204: both arguments variables
x = 8;
y = 2147483649;
result = (x - y);
check = -2147483641;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1205: both arguments constants
result = (8 - 2147483649)
check = -2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1206: LHS constant
y = 2147483649;
result = (8 - y)
check = -2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1207: RHS constant
x = 8;
result = (x - 2147483649)
check = -2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1208: both arguments variables
x = 8;
y = -2147483647;
result = (x - y);
check = 2147483655;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1209: both arguments constants
result = (8 - -2147483647)
check = 2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1210: LHS constant
y = -2147483647;
result = (8 - y)
check = 2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1211: RHS constant
x = 8;
result = (x - -2147483647)
check = 2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1212: both arguments variables
x = 8;
y = -2147483648;
result = (x - y);
check = 2147483656;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1213: both arguments constants
result = (8 - -2147483648)
check = 2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1214: LHS constant
y = -2147483648;
result = (8 - y)
check = 2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1215: RHS constant
x = 8;
result = (x - -2147483648)
check = 2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1216: both arguments variables
x = 8;
y = -2147483649;
result = (x - y);
check = 2147483657;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1217: both arguments constants
result = (8 - -2147483649)
check = 2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1218: LHS constant
y = -2147483649;
result = (8 - y)
check = 2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1219: RHS constant
x = 8;
result = (x - -2147483649)
check = 2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1220: both arguments variables
x = 8;
y = -2147483650;
result = (x - y);
check = 2147483658;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1221: both arguments constants
result = (8 - -2147483650)
check = 2147483658
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1222: LHS constant
y = -2147483650;
result = (8 - y)
check = 2147483658
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1223: RHS constant
x = 8;
result = (x - -2147483650)
check = 2147483658
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1224: both arguments variables
x = 8;
y = 4294967295;
result = (x - y);
check = -4294967287;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1225: both arguments constants
result = (8 - 4294967295)
check = -4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1226: LHS constant
y = 4294967295;
result = (8 - y)
check = -4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1227: RHS constant
x = 8;
result = (x - 4294967295)
check = -4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1228: both arguments variables
x = 8;
y = 4294967296;
result = (x - y);
check = -4294967288;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1229: both arguments constants
result = (8 - 4294967296)
check = -4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1230: LHS constant
y = 4294967296;
result = (8 - y)
check = -4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1231: RHS constant
x = 8;
result = (x - 4294967296)
check = -4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1232: both arguments variables
x = 8;
y = -4294967295;
result = (x - y);
check = 4294967303;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1233: both arguments constants
result = (8 - -4294967295)
check = 4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1234: LHS constant
y = -4294967295;
result = (8 - y)
check = 4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1235: RHS constant
x = 8;
result = (x - -4294967295)
check = 4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1236: both arguments variables
x = 8;
y = -4294967296;
result = (x - y);
check = 4294967304;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1237: both arguments constants
result = (8 - -4294967296)
check = 4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1238: LHS constant
y = -4294967296;
result = (8 - y)
check = 4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1239: RHS constant
x = 8;
result = (x - -4294967296)
check = 4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1240: both arguments variables
x = -8;
y = 0;
result = (x - y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1241: both arguments constants
result = (-8 - 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1242: LHS constant
y = 0;
result = (-8 - y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1243: RHS constant
x = -8;
result = (x - 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1244: both arguments variables
x = -8;
y = 1;
result = (x - y);
check = -9;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1245: both arguments constants
result = (-8 - 1)
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1246: LHS constant
y = 1;
result = (-8 - y)
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1247: RHS constant
x = -8;
result = (x - 1)
check = -9
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1248: both arguments variables
x = -8;
y = -1;
result = (x - y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1249: both arguments constants
result = (-8 - -1)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1250: LHS constant
y = -1;
result = (-8 - y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1251: RHS constant
x = -8;
result = (x - -1)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1252: both arguments variables
x = -8;
y = 2;
result = (x - y);
check = -10;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1253: both arguments constants
result = (-8 - 2)
check = -10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1254: LHS constant
y = 2;
result = (-8 - y)
check = -10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1255: RHS constant
x = -8;
result = (x - 2)
check = -10
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1256: both arguments variables
x = -8;
y = -2;
result = (x - y);
check = -6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1257: both arguments constants
result = (-8 - -2)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1258: LHS constant
y = -2;
result = (-8 - y)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1259: RHS constant
x = -8;
result = (x - -2)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1260: both arguments variables
x = -8;
y = 3;
result = (x - y);
check = -11;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1261: both arguments constants
result = (-8 - 3)
check = -11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1262: LHS constant
y = 3;
result = (-8 - y)
check = -11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1263: RHS constant
x = -8;
result = (x - 3)
check = -11
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1264: both arguments variables
x = -8;
y = -3;
result = (x - y);
check = -5;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1265: both arguments constants
result = (-8 - -3)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1266: LHS constant
y = -3;
result = (-8 - y)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1267: RHS constant
x = -8;
result = (x - -3)
check = -5
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1268: both arguments variables
x = -8;
y = 4;
result = (x - y);
check = -12;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1269: both arguments constants
result = (-8 - 4)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1270: LHS constant
y = 4;
result = (-8 - y)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1271: RHS constant
x = -8;
result = (x - 4)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1272: both arguments variables
x = -8;
y = -4;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1273: both arguments constants
result = (-8 - -4)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1274: LHS constant
y = -4;
result = (-8 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1275: RHS constant
x = -8;
result = (x - -4)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1276: both arguments variables
x = -8;
y = 8;
result = (x - y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1277: both arguments constants
result = (-8 - 8)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1278: LHS constant
y = 8;
result = (-8 - y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1279: RHS constant
x = -8;
result = (x - 8)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1280: both arguments variables
x = -8;
y = -8;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1281: both arguments constants
result = (-8 - -8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1282: LHS constant
y = -8;
result = (-8 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1283: RHS constant
x = -8;
result = (x - -8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1284: both arguments variables
x = -8;
y = 1073741822;
result = (x - y);
check = -1073741830;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1285: both arguments constants
result = (-8 - 1073741822)
check = -1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1286: LHS constant
y = 1073741822;
result = (-8 - y)
check = -1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1287: RHS constant
x = -8;
result = (x - 1073741822)
check = -1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1288: both arguments variables
x = -8;
y = 1073741823;
result = (x - y);
check = -1073741831;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1289: both arguments constants
result = (-8 - 1073741823)
check = -1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1290: LHS constant
y = 1073741823;
result = (-8 - y)
check = -1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1291: RHS constant
x = -8;
result = (x - 1073741823)
check = -1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1292: both arguments variables
x = -8;
y = 1073741824;
result = (x - y);
check = -1073741832;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1293: both arguments constants
result = (-8 - 1073741824)
check = -1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1294: LHS constant
y = 1073741824;
result = (-8 - y)
check = -1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1295: RHS constant
x = -8;
result = (x - 1073741824)
check = -1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1296: both arguments variables
x = -8;
y = 1073741825;
result = (x - y);
check = -1073741833;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1297: both arguments constants
result = (-8 - 1073741825)
check = -1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1298: LHS constant
y = 1073741825;
result = (-8 - y)
check = -1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1299: RHS constant
x = -8;
result = (x - 1073741825)
check = -1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test13() {
var x;
var y;
var result;
var check;
// Test 1300: both arguments variables
x = -8;
y = -1073741823;
result = (x - y);
check = 1073741815;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1301: both arguments constants
result = (-8 - -1073741823)
check = 1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1302: LHS constant
y = -1073741823;
result = (-8 - y)
check = 1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1303: RHS constant
x = -8;
result = (x - -1073741823)
check = 1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1304: both arguments variables
x = -8;
y = (-0x3fffffff-1);
result = (x - y);
check = 1073741816;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1305: both arguments constants
result = (-8 - (-0x3fffffff-1))
check = 1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1306: LHS constant
y = (-0x3fffffff-1);
result = (-8 - y)
check = 1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1307: RHS constant
x = -8;
result = (x - (-0x3fffffff-1))
check = 1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1308: both arguments variables
x = -8;
y = -1073741825;
result = (x - y);
check = 1073741817;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1309: both arguments constants
result = (-8 - -1073741825)
check = 1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1310: LHS constant
y = -1073741825;
result = (-8 - y)
check = 1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1311: RHS constant
x = -8;
result = (x - -1073741825)
check = 1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1312: both arguments variables
x = -8;
y = -1073741826;
result = (x - y);
check = 1073741818;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1313: both arguments constants
result = (-8 - -1073741826)
check = 1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1314: LHS constant
y = -1073741826;
result = (-8 - y)
check = 1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1315: RHS constant
x = -8;
result = (x - -1073741826)
check = 1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1316: both arguments variables
x = -8;
y = 2147483646;
result = (x - y);
check = -2147483654;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1317: both arguments constants
result = (-8 - 2147483646)
check = -2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1318: LHS constant
y = 2147483646;
result = (-8 - y)
check = -2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1319: RHS constant
x = -8;
result = (x - 2147483646)
check = -2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1320: both arguments variables
x = -8;
y = 2147483647;
result = (x - y);
check = -2147483655;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1321: both arguments constants
result = (-8 - 2147483647)
check = -2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1322: LHS constant
y = 2147483647;
result = (-8 - y)
check = -2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1323: RHS constant
x = -8;
result = (x - 2147483647)
check = -2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1324: both arguments variables
x = -8;
y = 2147483648;
result = (x - y);
check = -2147483656;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1325: both arguments constants
result = (-8 - 2147483648)
check = -2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1326: LHS constant
y = 2147483648;
result = (-8 - y)
check = -2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1327: RHS constant
x = -8;
result = (x - 2147483648)
check = -2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1328: both arguments variables
x = -8;
y = 2147483649;
result = (x - y);
check = -2147483657;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1329: both arguments constants
result = (-8 - 2147483649)
check = -2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1330: LHS constant
y = 2147483649;
result = (-8 - y)
check = -2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1331: RHS constant
x = -8;
result = (x - 2147483649)
check = -2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1332: both arguments variables
x = -8;
y = -2147483647;
result = (x - y);
check = 2147483639;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1333: both arguments constants
result = (-8 - -2147483647)
check = 2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1334: LHS constant
y = -2147483647;
result = (-8 - y)
check = 2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1335: RHS constant
x = -8;
result = (x - -2147483647)
check = 2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1336: both arguments variables
x = -8;
y = -2147483648;
result = (x - y);
check = 2147483640;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1337: both arguments constants
result = (-8 - -2147483648)
check = 2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1338: LHS constant
y = -2147483648;
result = (-8 - y)
check = 2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1339: RHS constant
x = -8;
result = (x - -2147483648)
check = 2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1340: both arguments variables
x = -8;
y = -2147483649;
result = (x - y);
check = 2147483641;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1341: both arguments constants
result = (-8 - -2147483649)
check = 2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1342: LHS constant
y = -2147483649;
result = (-8 - y)
check = 2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1343: RHS constant
x = -8;
result = (x - -2147483649)
check = 2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1344: both arguments variables
x = -8;
y = -2147483650;
result = (x - y);
check = 2147483642;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1345: both arguments constants
result = (-8 - -2147483650)
check = 2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1346: LHS constant
y = -2147483650;
result = (-8 - y)
check = 2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1347: RHS constant
x = -8;
result = (x - -2147483650)
check = 2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1348: both arguments variables
x = -8;
y = 4294967295;
result = (x - y);
check = -4294967303;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1349: both arguments constants
result = (-8 - 4294967295)
check = -4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1350: LHS constant
y = 4294967295;
result = (-8 - y)
check = -4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1351: RHS constant
x = -8;
result = (x - 4294967295)
check = -4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1352: both arguments variables
x = -8;
y = 4294967296;
result = (x - y);
check = -4294967304;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1353: both arguments constants
result = (-8 - 4294967296)
check = -4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1354: LHS constant
y = 4294967296;
result = (-8 - y)
check = -4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1355: RHS constant
x = -8;
result = (x - 4294967296)
check = -4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1356: both arguments variables
x = -8;
y = -4294967295;
result = (x - y);
check = 4294967287;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1357: both arguments constants
result = (-8 - -4294967295)
check = 4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1358: LHS constant
y = -4294967295;
result = (-8 - y)
check = 4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1359: RHS constant
x = -8;
result = (x - -4294967295)
check = 4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1360: both arguments variables
x = -8;
y = -4294967296;
result = (x - y);
check = 4294967288;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1361: both arguments constants
result = (-8 - -4294967296)
check = 4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1362: LHS constant
y = -4294967296;
result = (-8 - y)
check = 4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1363: RHS constant
x = -8;
result = (x - -4294967296)
check = 4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1364: both arguments variables
x = 1073741822;
y = 0;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1365: both arguments constants
result = (1073741822 - 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1366: LHS constant
y = 0;
result = (1073741822 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1367: RHS constant
x = 1073741822;
result = (x - 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1368: both arguments variables
x = 1073741822;
y = 1;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1369: both arguments constants
result = (1073741822 - 1)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1370: LHS constant
y = 1;
result = (1073741822 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1371: RHS constant
x = 1073741822;
result = (x - 1)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1372: both arguments variables
x = 1073741822;
y = -1;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1373: both arguments constants
result = (1073741822 - -1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1374: LHS constant
y = -1;
result = (1073741822 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1375: RHS constant
x = 1073741822;
result = (x - -1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1376: both arguments variables
x = 1073741822;
y = 2;
result = (x - y);
check = 1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1377: both arguments constants
result = (1073741822 - 2)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1378: LHS constant
y = 2;
result = (1073741822 - y)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1379: RHS constant
x = 1073741822;
result = (x - 2)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1380: both arguments variables
x = 1073741822;
y = -2;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1381: both arguments constants
result = (1073741822 - -2)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1382: LHS constant
y = -2;
result = (1073741822 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1383: RHS constant
x = 1073741822;
result = (x - -2)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1384: both arguments variables
x = 1073741822;
y = 3;
result = (x - y);
check = 1073741819;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1385: both arguments constants
result = (1073741822 - 3)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1386: LHS constant
y = 3;
result = (1073741822 - y)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1387: RHS constant
x = 1073741822;
result = (x - 3)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1388: both arguments variables
x = 1073741822;
y = -3;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1389: both arguments constants
result = (1073741822 - -3)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1390: LHS constant
y = -3;
result = (1073741822 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1391: RHS constant
x = 1073741822;
result = (x - -3)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1392: both arguments variables
x = 1073741822;
y = 4;
result = (x - y);
check = 1073741818;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1393: both arguments constants
result = (1073741822 - 4)
check = 1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1394: LHS constant
y = 4;
result = (1073741822 - y)
check = 1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1395: RHS constant
x = 1073741822;
result = (x - 4)
check = 1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1396: both arguments variables
x = 1073741822;
y = -4;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1397: both arguments constants
result = (1073741822 - -4)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1398: LHS constant
y = -4;
result = (1073741822 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1399: RHS constant
x = 1073741822;
result = (x - -4)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test14() {
var x;
var y;
var result;
var check;
// Test 1400: both arguments variables
x = 1073741822;
y = 8;
result = (x - y);
check = 1073741814;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1401: both arguments constants
result = (1073741822 - 8)
check = 1073741814
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1402: LHS constant
y = 8;
result = (1073741822 - y)
check = 1073741814
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1403: RHS constant
x = 1073741822;
result = (x - 8)
check = 1073741814
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1404: both arguments variables
x = 1073741822;
y = -8;
result = (x - y);
check = 1073741830;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1405: both arguments constants
result = (1073741822 - -8)
check = 1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1406: LHS constant
y = -8;
result = (1073741822 - y)
check = 1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1407: RHS constant
x = 1073741822;
result = (x - -8)
check = 1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1408: both arguments variables
x = 1073741822;
y = 1073741822;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1409: both arguments constants
result = (1073741822 - 1073741822)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1410: LHS constant
y = 1073741822;
result = (1073741822 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1411: RHS constant
x = 1073741822;
result = (x - 1073741822)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1412: both arguments variables
x = 1073741822;
y = 1073741823;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1413: both arguments constants
result = (1073741822 - 1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1414: LHS constant
y = 1073741823;
result = (1073741822 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1415: RHS constant
x = 1073741822;
result = (x - 1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1416: both arguments variables
x = 1073741822;
y = 1073741824;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1417: both arguments constants
result = (1073741822 - 1073741824)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1418: LHS constant
y = 1073741824;
result = (1073741822 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1419: RHS constant
x = 1073741822;
result = (x - 1073741824)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1420: both arguments variables
x = 1073741822;
y = 1073741825;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1421: both arguments constants
result = (1073741822 - 1073741825)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1422: LHS constant
y = 1073741825;
result = (1073741822 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1423: RHS constant
x = 1073741822;
result = (x - 1073741825)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1424: both arguments variables
x = 1073741822;
y = -1073741823;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1425: both arguments constants
result = (1073741822 - -1073741823)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1426: LHS constant
y = -1073741823;
result = (1073741822 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1427: RHS constant
x = 1073741822;
result = (x - -1073741823)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1428: both arguments variables
x = 1073741822;
y = (-0x3fffffff-1);
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1429: both arguments constants
result = (1073741822 - (-0x3fffffff-1))
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1430: LHS constant
y = (-0x3fffffff-1);
result = (1073741822 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1431: RHS constant
x = 1073741822;
result = (x - (-0x3fffffff-1))
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1432: both arguments variables
x = 1073741822;
y = -1073741825;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1433: both arguments constants
result = (1073741822 - -1073741825)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1434: LHS constant
y = -1073741825;
result = (1073741822 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1435: RHS constant
x = 1073741822;
result = (x - -1073741825)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1436: both arguments variables
x = 1073741822;
y = -1073741826;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1437: both arguments constants
result = (1073741822 - -1073741826)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1438: LHS constant
y = -1073741826;
result = (1073741822 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1439: RHS constant
x = 1073741822;
result = (x - -1073741826)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1440: both arguments variables
x = 1073741822;
y = 2147483646;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1441: both arguments constants
result = (1073741822 - 2147483646)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1442: LHS constant
y = 2147483646;
result = (1073741822 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1443: RHS constant
x = 1073741822;
result = (x - 2147483646)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1444: both arguments variables
x = 1073741822;
y = 2147483647;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1445: both arguments constants
result = (1073741822 - 2147483647)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1446: LHS constant
y = 2147483647;
result = (1073741822 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1447: RHS constant
x = 1073741822;
result = (x - 2147483647)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1448: both arguments variables
x = 1073741822;
y = 2147483648;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1449: both arguments constants
result = (1073741822 - 2147483648)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1450: LHS constant
y = 2147483648;
result = (1073741822 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1451: RHS constant
x = 1073741822;
result = (x - 2147483648)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1452: both arguments variables
x = 1073741822;
y = 2147483649;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1453: both arguments constants
result = (1073741822 - 2147483649)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1454: LHS constant
y = 2147483649;
result = (1073741822 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1455: RHS constant
x = 1073741822;
result = (x - 2147483649)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1456: both arguments variables
x = 1073741822;
y = -2147483647;
result = (x - y);
check = 3221225469;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1457: both arguments constants
result = (1073741822 - -2147483647)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1458: LHS constant
y = -2147483647;
result = (1073741822 - y)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1459: RHS constant
x = 1073741822;
result = (x - -2147483647)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1460: both arguments variables
x = 1073741822;
y = -2147483648;
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1461: both arguments constants
result = (1073741822 - -2147483648)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1462: LHS constant
y = -2147483648;
result = (1073741822 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1463: RHS constant
x = 1073741822;
result = (x - -2147483648)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1464: both arguments variables
x = 1073741822;
y = -2147483649;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1465: both arguments constants
result = (1073741822 - -2147483649)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1466: LHS constant
y = -2147483649;
result = (1073741822 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1467: RHS constant
x = 1073741822;
result = (x - -2147483649)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1468: both arguments variables
x = 1073741822;
y = -2147483650;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1469: both arguments constants
result = (1073741822 - -2147483650)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1470: LHS constant
y = -2147483650;
result = (1073741822 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1471: RHS constant
x = 1073741822;
result = (x - -2147483650)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1472: both arguments variables
x = 1073741822;
y = 4294967295;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1473: both arguments constants
result = (1073741822 - 4294967295)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1474: LHS constant
y = 4294967295;
result = (1073741822 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1475: RHS constant
x = 1073741822;
result = (x - 4294967295)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1476: both arguments variables
x = 1073741822;
y = 4294967296;
result = (x - y);
check = -3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1477: both arguments constants
result = (1073741822 - 4294967296)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1478: LHS constant
y = 4294967296;
result = (1073741822 - y)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1479: RHS constant
x = 1073741822;
result = (x - 4294967296)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1480: both arguments variables
x = 1073741822;
y = -4294967295;
result = (x - y);
check = 5368709117;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1481: both arguments constants
result = (1073741822 - -4294967295)
check = 5368709117
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1482: LHS constant
y = -4294967295;
result = (1073741822 - y)
check = 5368709117
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1483: RHS constant
x = 1073741822;
result = (x - -4294967295)
check = 5368709117
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1484: both arguments variables
x = 1073741822;
y = -4294967296;
result = (x - y);
check = 5368709118;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1485: both arguments constants
result = (1073741822 - -4294967296)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1486: LHS constant
y = -4294967296;
result = (1073741822 - y)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1487: RHS constant
x = 1073741822;
result = (x - -4294967296)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1488: both arguments variables
x = 1073741823;
y = 0;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1489: both arguments constants
result = (1073741823 - 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1490: LHS constant
y = 0;
result = (1073741823 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1491: RHS constant
x = 1073741823;
result = (x - 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1492: both arguments variables
x = 1073741823;
y = 1;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1493: both arguments constants
result = (1073741823 - 1)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1494: LHS constant
y = 1;
result = (1073741823 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1495: RHS constant
x = 1073741823;
result = (x - 1)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1496: both arguments variables
x = 1073741823;
y = -1;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1497: both arguments constants
result = (1073741823 - -1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1498: LHS constant
y = -1;
result = (1073741823 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1499: RHS constant
x = 1073741823;
result = (x - -1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test15() {
var x;
var y;
var result;
var check;
// Test 1500: both arguments variables
x = 1073741823;
y = 2;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1501: both arguments constants
result = (1073741823 - 2)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1502: LHS constant
y = 2;
result = (1073741823 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1503: RHS constant
x = 1073741823;
result = (x - 2)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1504: both arguments variables
x = 1073741823;
y = -2;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1505: both arguments constants
result = (1073741823 - -2)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1506: LHS constant
y = -2;
result = (1073741823 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1507: RHS constant
x = 1073741823;
result = (x - -2)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1508: both arguments variables
x = 1073741823;
y = 3;
result = (x - y);
check = 1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1509: both arguments constants
result = (1073741823 - 3)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1510: LHS constant
y = 3;
result = (1073741823 - y)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1511: RHS constant
x = 1073741823;
result = (x - 3)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1512: both arguments variables
x = 1073741823;
y = -3;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1513: both arguments constants
result = (1073741823 - -3)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1514: LHS constant
y = -3;
result = (1073741823 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1515: RHS constant
x = 1073741823;
result = (x - -3)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1516: both arguments variables
x = 1073741823;
y = 4;
result = (x - y);
check = 1073741819;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1517: both arguments constants
result = (1073741823 - 4)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1518: LHS constant
y = 4;
result = (1073741823 - y)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1519: RHS constant
x = 1073741823;
result = (x - 4)
check = 1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1520: both arguments variables
x = 1073741823;
y = -4;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1521: both arguments constants
result = (1073741823 - -4)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1522: LHS constant
y = -4;
result = (1073741823 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1523: RHS constant
x = 1073741823;
result = (x - -4)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1524: both arguments variables
x = 1073741823;
y = 8;
result = (x - y);
check = 1073741815;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1525: both arguments constants
result = (1073741823 - 8)
check = 1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1526: LHS constant
y = 8;
result = (1073741823 - y)
check = 1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1527: RHS constant
x = 1073741823;
result = (x - 8)
check = 1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1528: both arguments variables
x = 1073741823;
y = -8;
result = (x - y);
check = 1073741831;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1529: both arguments constants
result = (1073741823 - -8)
check = 1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1530: LHS constant
y = -8;
result = (1073741823 - y)
check = 1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1531: RHS constant
x = 1073741823;
result = (x - -8)
check = 1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1532: both arguments variables
x = 1073741823;
y = 1073741822;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1533: both arguments constants
result = (1073741823 - 1073741822)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1534: LHS constant
y = 1073741822;
result = (1073741823 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1535: RHS constant
x = 1073741823;
result = (x - 1073741822)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1536: both arguments variables
x = 1073741823;
y = 1073741823;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1537: both arguments constants
result = (1073741823 - 1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1538: LHS constant
y = 1073741823;
result = (1073741823 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1539: RHS constant
x = 1073741823;
result = (x - 1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1540: both arguments variables
x = 1073741823;
y = 1073741824;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1541: both arguments constants
result = (1073741823 - 1073741824)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1542: LHS constant
y = 1073741824;
result = (1073741823 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1543: RHS constant
x = 1073741823;
result = (x - 1073741824)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1544: both arguments variables
x = 1073741823;
y = 1073741825;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1545: both arguments constants
result = (1073741823 - 1073741825)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1546: LHS constant
y = 1073741825;
result = (1073741823 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1547: RHS constant
x = 1073741823;
result = (x - 1073741825)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1548: both arguments variables
x = 1073741823;
y = -1073741823;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1549: both arguments constants
result = (1073741823 - -1073741823)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1550: LHS constant
y = -1073741823;
result = (1073741823 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1551: RHS constant
x = 1073741823;
result = (x - -1073741823)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1552: both arguments variables
x = 1073741823;
y = (-0x3fffffff-1);
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1553: both arguments constants
result = (1073741823 - (-0x3fffffff-1))
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1554: LHS constant
y = (-0x3fffffff-1);
result = (1073741823 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1555: RHS constant
x = 1073741823;
result = (x - (-0x3fffffff-1))
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1556: both arguments variables
x = 1073741823;
y = -1073741825;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1557: both arguments constants
result = (1073741823 - -1073741825)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1558: LHS constant
y = -1073741825;
result = (1073741823 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1559: RHS constant
x = 1073741823;
result = (x - -1073741825)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1560: both arguments variables
x = 1073741823;
y = -1073741826;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1561: both arguments constants
result = (1073741823 - -1073741826)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1562: LHS constant
y = -1073741826;
result = (1073741823 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1563: RHS constant
x = 1073741823;
result = (x - -1073741826)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1564: both arguments variables
x = 1073741823;
y = 2147483646;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1565: both arguments constants
result = (1073741823 - 2147483646)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1566: LHS constant
y = 2147483646;
result = (1073741823 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1567: RHS constant
x = 1073741823;
result = (x - 2147483646)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1568: both arguments variables
x = 1073741823;
y = 2147483647;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1569: both arguments constants
result = (1073741823 - 2147483647)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1570: LHS constant
y = 2147483647;
result = (1073741823 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1571: RHS constant
x = 1073741823;
result = (x - 2147483647)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1572: both arguments variables
x = 1073741823;
y = 2147483648;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1573: both arguments constants
result = (1073741823 - 2147483648)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1574: LHS constant
y = 2147483648;
result = (1073741823 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1575: RHS constant
x = 1073741823;
result = (x - 2147483648)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1576: both arguments variables
x = 1073741823;
y = 2147483649;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1577: both arguments constants
result = (1073741823 - 2147483649)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1578: LHS constant
y = 2147483649;
result = (1073741823 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1579: RHS constant
x = 1073741823;
result = (x - 2147483649)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1580: both arguments variables
x = 1073741823;
y = -2147483647;
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1581: both arguments constants
result = (1073741823 - -2147483647)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1582: LHS constant
y = -2147483647;
result = (1073741823 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1583: RHS constant
x = 1073741823;
result = (x - -2147483647)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1584: both arguments variables
x = 1073741823;
y = -2147483648;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1585: both arguments constants
result = (1073741823 - -2147483648)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1586: LHS constant
y = -2147483648;
result = (1073741823 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1587: RHS constant
x = 1073741823;
result = (x - -2147483648)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1588: both arguments variables
x = 1073741823;
y = -2147483649;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1589: both arguments constants
result = (1073741823 - -2147483649)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1590: LHS constant
y = -2147483649;
result = (1073741823 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1591: RHS constant
x = 1073741823;
result = (x - -2147483649)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1592: both arguments variables
x = 1073741823;
y = -2147483650;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1593: both arguments constants
result = (1073741823 - -2147483650)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1594: LHS constant
y = -2147483650;
result = (1073741823 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1595: RHS constant
x = 1073741823;
result = (x - -2147483650)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1596: both arguments variables
x = 1073741823;
y = 4294967295;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1597: both arguments constants
result = (1073741823 - 4294967295)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1598: LHS constant
y = 4294967295;
result = (1073741823 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1599: RHS constant
x = 1073741823;
result = (x - 4294967295)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test16() {
var x;
var y;
var result;
var check;
// Test 1600: both arguments variables
x = 1073741823;
y = 4294967296;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1601: both arguments constants
result = (1073741823 - 4294967296)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1602: LHS constant
y = 4294967296;
result = (1073741823 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1603: RHS constant
x = 1073741823;
result = (x - 4294967296)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1604: both arguments variables
x = 1073741823;
y = -4294967295;
result = (x - y);
check = 5368709118;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1605: both arguments constants
result = (1073741823 - -4294967295)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1606: LHS constant
y = -4294967295;
result = (1073741823 - y)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1607: RHS constant
x = 1073741823;
result = (x - -4294967295)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1608: both arguments variables
x = 1073741823;
y = -4294967296;
result = (x - y);
check = 5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1609: both arguments constants
result = (1073741823 - -4294967296)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1610: LHS constant
y = -4294967296;
result = (1073741823 - y)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1611: RHS constant
x = 1073741823;
result = (x - -4294967296)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1612: both arguments variables
x = 1073741824;
y = 0;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1613: both arguments constants
result = (1073741824 - 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1614: LHS constant
y = 0;
result = (1073741824 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1615: RHS constant
x = 1073741824;
result = (x - 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1616: both arguments variables
x = 1073741824;
y = 1;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1617: both arguments constants
result = (1073741824 - 1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1618: LHS constant
y = 1;
result = (1073741824 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1619: RHS constant
x = 1073741824;
result = (x - 1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1620: both arguments variables
x = 1073741824;
y = -1;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1621: both arguments constants
result = (1073741824 - -1)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1622: LHS constant
y = -1;
result = (1073741824 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1623: RHS constant
x = 1073741824;
result = (x - -1)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1624: both arguments variables
x = 1073741824;
y = 2;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1625: both arguments constants
result = (1073741824 - 2)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1626: LHS constant
y = 2;
result = (1073741824 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1627: RHS constant
x = 1073741824;
result = (x - 2)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1628: both arguments variables
x = 1073741824;
y = -2;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1629: both arguments constants
result = (1073741824 - -2)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1630: LHS constant
y = -2;
result = (1073741824 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1631: RHS constant
x = 1073741824;
result = (x - -2)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1632: both arguments variables
x = 1073741824;
y = 3;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1633: both arguments constants
result = (1073741824 - 3)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1634: LHS constant
y = 3;
result = (1073741824 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1635: RHS constant
x = 1073741824;
result = (x - 3)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1636: both arguments variables
x = 1073741824;
y = -3;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1637: both arguments constants
result = (1073741824 - -3)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1638: LHS constant
y = -3;
result = (1073741824 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1639: RHS constant
x = 1073741824;
result = (x - -3)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1640: both arguments variables
x = 1073741824;
y = 4;
result = (x - y);
check = 1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1641: both arguments constants
result = (1073741824 - 4)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1642: LHS constant
y = 4;
result = (1073741824 - y)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1643: RHS constant
x = 1073741824;
result = (x - 4)
check = 1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1644: both arguments variables
x = 1073741824;
y = -4;
result = (x - y);
check = 1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1645: both arguments constants
result = (1073741824 - -4)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1646: LHS constant
y = -4;
result = (1073741824 - y)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1647: RHS constant
x = 1073741824;
result = (x - -4)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1648: both arguments variables
x = 1073741824;
y = 8;
result = (x - y);
check = 1073741816;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1649: both arguments constants
result = (1073741824 - 8)
check = 1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1650: LHS constant
y = 8;
result = (1073741824 - y)
check = 1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1651: RHS constant
x = 1073741824;
result = (x - 8)
check = 1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1652: both arguments variables
x = 1073741824;
y = -8;
result = (x - y);
check = 1073741832;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1653: both arguments constants
result = (1073741824 - -8)
check = 1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1654: LHS constant
y = -8;
result = (1073741824 - y)
check = 1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1655: RHS constant
x = 1073741824;
result = (x - -8)
check = 1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1656: both arguments variables
x = 1073741824;
y = 1073741822;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1657: both arguments constants
result = (1073741824 - 1073741822)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1658: LHS constant
y = 1073741822;
result = (1073741824 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1659: RHS constant
x = 1073741824;
result = (x - 1073741822)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1660: both arguments variables
x = 1073741824;
y = 1073741823;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1661: both arguments constants
result = (1073741824 - 1073741823)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1662: LHS constant
y = 1073741823;
result = (1073741824 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1663: RHS constant
x = 1073741824;
result = (x - 1073741823)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1664: both arguments variables
x = 1073741824;
y = 1073741824;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1665: both arguments constants
result = (1073741824 - 1073741824)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1666: LHS constant
y = 1073741824;
result = (1073741824 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1667: RHS constant
x = 1073741824;
result = (x - 1073741824)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1668: both arguments variables
x = 1073741824;
y = 1073741825;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1669: both arguments constants
result = (1073741824 - 1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1670: LHS constant
y = 1073741825;
result = (1073741824 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1671: RHS constant
x = 1073741824;
result = (x - 1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1672: both arguments variables
x = 1073741824;
y = -1073741823;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1673: both arguments constants
result = (1073741824 - -1073741823)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1674: LHS constant
y = -1073741823;
result = (1073741824 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1675: RHS constant
x = 1073741824;
result = (x - -1073741823)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1676: both arguments variables
x = 1073741824;
y = (-0x3fffffff-1);
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1677: both arguments constants
result = (1073741824 - (-0x3fffffff-1))
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1678: LHS constant
y = (-0x3fffffff-1);
result = (1073741824 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1679: RHS constant
x = 1073741824;
result = (x - (-0x3fffffff-1))
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1680: both arguments variables
x = 1073741824;
y = -1073741825;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1681: both arguments constants
result = (1073741824 - -1073741825)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1682: LHS constant
y = -1073741825;
result = (1073741824 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1683: RHS constant
x = 1073741824;
result = (x - -1073741825)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1684: both arguments variables
x = 1073741824;
y = -1073741826;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1685: both arguments constants
result = (1073741824 - -1073741826)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1686: LHS constant
y = -1073741826;
result = (1073741824 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1687: RHS constant
x = 1073741824;
result = (x - -1073741826)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1688: both arguments variables
x = 1073741824;
y = 2147483646;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1689: both arguments constants
result = (1073741824 - 2147483646)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1690: LHS constant
y = 2147483646;
result = (1073741824 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1691: RHS constant
x = 1073741824;
result = (x - 2147483646)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1692: both arguments variables
x = 1073741824;
y = 2147483647;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1693: both arguments constants
result = (1073741824 - 2147483647)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1694: LHS constant
y = 2147483647;
result = (1073741824 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1695: RHS constant
x = 1073741824;
result = (x - 2147483647)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1696: both arguments variables
x = 1073741824;
y = 2147483648;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1697: both arguments constants
result = (1073741824 - 2147483648)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1698: LHS constant
y = 2147483648;
result = (1073741824 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1699: RHS constant
x = 1073741824;
result = (x - 2147483648)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test17() {
var x;
var y;
var result;
var check;
// Test 1700: both arguments variables
x = 1073741824;
y = 2147483649;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1701: both arguments constants
result = (1073741824 - 2147483649)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1702: LHS constant
y = 2147483649;
result = (1073741824 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1703: RHS constant
x = 1073741824;
result = (x - 2147483649)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1704: both arguments variables
x = 1073741824;
y = -2147483647;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1705: both arguments constants
result = (1073741824 - -2147483647)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1706: LHS constant
y = -2147483647;
result = (1073741824 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1707: RHS constant
x = 1073741824;
result = (x - -2147483647)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1708: both arguments variables
x = 1073741824;
y = -2147483648;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1709: both arguments constants
result = (1073741824 - -2147483648)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1710: LHS constant
y = -2147483648;
result = (1073741824 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1711: RHS constant
x = 1073741824;
result = (x - -2147483648)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1712: both arguments variables
x = 1073741824;
y = -2147483649;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1713: both arguments constants
result = (1073741824 - -2147483649)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1714: LHS constant
y = -2147483649;
result = (1073741824 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1715: RHS constant
x = 1073741824;
result = (x - -2147483649)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1716: both arguments variables
x = 1073741824;
y = -2147483650;
result = (x - y);
check = 3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1717: both arguments constants
result = (1073741824 - -2147483650)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1718: LHS constant
y = -2147483650;
result = (1073741824 - y)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1719: RHS constant
x = 1073741824;
result = (x - -2147483650)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1720: both arguments variables
x = 1073741824;
y = 4294967295;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1721: both arguments constants
result = (1073741824 - 4294967295)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1722: LHS constant
y = 4294967295;
result = (1073741824 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1723: RHS constant
x = 1073741824;
result = (x - 4294967295)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1724: both arguments variables
x = 1073741824;
y = 4294967296;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1725: both arguments constants
result = (1073741824 - 4294967296)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1726: LHS constant
y = 4294967296;
result = (1073741824 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1727: RHS constant
x = 1073741824;
result = (x - 4294967296)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1728: both arguments variables
x = 1073741824;
y = -4294967295;
result = (x - y);
check = 5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1729: both arguments constants
result = (1073741824 - -4294967295)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1730: LHS constant
y = -4294967295;
result = (1073741824 - y)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1731: RHS constant
x = 1073741824;
result = (x - -4294967295)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1732: both arguments variables
x = 1073741824;
y = -4294967296;
result = (x - y);
check = 5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1733: both arguments constants
result = (1073741824 - -4294967296)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1734: LHS constant
y = -4294967296;
result = (1073741824 - y)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1735: RHS constant
x = 1073741824;
result = (x - -4294967296)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1736: both arguments variables
x = 1073741825;
y = 0;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1737: both arguments constants
result = (1073741825 - 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1738: LHS constant
y = 0;
result = (1073741825 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1739: RHS constant
x = 1073741825;
result = (x - 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1740: both arguments variables
x = 1073741825;
y = 1;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1741: both arguments constants
result = (1073741825 - 1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1742: LHS constant
y = 1;
result = (1073741825 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1743: RHS constant
x = 1073741825;
result = (x - 1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1744: both arguments variables
x = 1073741825;
y = -1;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1745: both arguments constants
result = (1073741825 - -1)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1746: LHS constant
y = -1;
result = (1073741825 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1747: RHS constant
x = 1073741825;
result = (x - -1)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1748: both arguments variables
x = 1073741825;
y = 2;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1749: both arguments constants
result = (1073741825 - 2)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1750: LHS constant
y = 2;
result = (1073741825 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1751: RHS constant
x = 1073741825;
result = (x - 2)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1752: both arguments variables
x = 1073741825;
y = -2;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1753: both arguments constants
result = (1073741825 - -2)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1754: LHS constant
y = -2;
result = (1073741825 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1755: RHS constant
x = 1073741825;
result = (x - -2)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1756: both arguments variables
x = 1073741825;
y = 3;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1757: both arguments constants
result = (1073741825 - 3)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1758: LHS constant
y = 3;
result = (1073741825 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1759: RHS constant
x = 1073741825;
result = (x - 3)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1760: both arguments variables
x = 1073741825;
y = -3;
result = (x - y);
check = 1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1761: both arguments constants
result = (1073741825 - -3)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1762: LHS constant
y = -3;
result = (1073741825 - y)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1763: RHS constant
x = 1073741825;
result = (x - -3)
check = 1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1764: both arguments variables
x = 1073741825;
y = 4;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1765: both arguments constants
result = (1073741825 - 4)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1766: LHS constant
y = 4;
result = (1073741825 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1767: RHS constant
x = 1073741825;
result = (x - 4)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1768: both arguments variables
x = 1073741825;
y = -4;
result = (x - y);
check = 1073741829;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1769: both arguments constants
result = (1073741825 - -4)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1770: LHS constant
y = -4;
result = (1073741825 - y)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1771: RHS constant
x = 1073741825;
result = (x - -4)
check = 1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1772: both arguments variables
x = 1073741825;
y = 8;
result = (x - y);
check = 1073741817;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1773: both arguments constants
result = (1073741825 - 8)
check = 1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1774: LHS constant
y = 8;
result = (1073741825 - y)
check = 1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1775: RHS constant
x = 1073741825;
result = (x - 8)
check = 1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1776: both arguments variables
x = 1073741825;
y = -8;
result = (x - y);
check = 1073741833;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1777: both arguments constants
result = (1073741825 - -8)
check = 1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1778: LHS constant
y = -8;
result = (1073741825 - y)
check = 1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1779: RHS constant
x = 1073741825;
result = (x - -8)
check = 1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1780: both arguments variables
x = 1073741825;
y = 1073741822;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1781: both arguments constants
result = (1073741825 - 1073741822)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1782: LHS constant
y = 1073741822;
result = (1073741825 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1783: RHS constant
x = 1073741825;
result = (x - 1073741822)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1784: both arguments variables
x = 1073741825;
y = 1073741823;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1785: both arguments constants
result = (1073741825 - 1073741823)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1786: LHS constant
y = 1073741823;
result = (1073741825 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1787: RHS constant
x = 1073741825;
result = (x - 1073741823)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1788: both arguments variables
x = 1073741825;
y = 1073741824;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1789: both arguments constants
result = (1073741825 - 1073741824)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1790: LHS constant
y = 1073741824;
result = (1073741825 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1791: RHS constant
x = 1073741825;
result = (x - 1073741824)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1792: both arguments variables
x = 1073741825;
y = 1073741825;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1793: both arguments constants
result = (1073741825 - 1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1794: LHS constant
y = 1073741825;
result = (1073741825 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1795: RHS constant
x = 1073741825;
result = (x - 1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1796: both arguments variables
x = 1073741825;
y = -1073741823;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1797: both arguments constants
result = (1073741825 - -1073741823)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1798: LHS constant
y = -1073741823;
result = (1073741825 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1799: RHS constant
x = 1073741825;
result = (x - -1073741823)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test18() {
var x;
var y;
var result;
var check;
// Test 1800: both arguments variables
x = 1073741825;
y = (-0x3fffffff-1);
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1801: both arguments constants
result = (1073741825 - (-0x3fffffff-1))
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1802: LHS constant
y = (-0x3fffffff-1);
result = (1073741825 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1803: RHS constant
x = 1073741825;
result = (x - (-0x3fffffff-1))
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1804: both arguments variables
x = 1073741825;
y = -1073741825;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1805: both arguments constants
result = (1073741825 - -1073741825)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1806: LHS constant
y = -1073741825;
result = (1073741825 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1807: RHS constant
x = 1073741825;
result = (x - -1073741825)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1808: both arguments variables
x = 1073741825;
y = -1073741826;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1809: both arguments constants
result = (1073741825 - -1073741826)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1810: LHS constant
y = -1073741826;
result = (1073741825 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1811: RHS constant
x = 1073741825;
result = (x - -1073741826)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1812: both arguments variables
x = 1073741825;
y = 2147483646;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1813: both arguments constants
result = (1073741825 - 2147483646)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1814: LHS constant
y = 2147483646;
result = (1073741825 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1815: RHS constant
x = 1073741825;
result = (x - 2147483646)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1816: both arguments variables
x = 1073741825;
y = 2147483647;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1817: both arguments constants
result = (1073741825 - 2147483647)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1818: LHS constant
y = 2147483647;
result = (1073741825 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1819: RHS constant
x = 1073741825;
result = (x - 2147483647)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1820: both arguments variables
x = 1073741825;
y = 2147483648;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1821: both arguments constants
result = (1073741825 - 2147483648)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1822: LHS constant
y = 2147483648;
result = (1073741825 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1823: RHS constant
x = 1073741825;
result = (x - 2147483648)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1824: both arguments variables
x = 1073741825;
y = 2147483649;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1825: both arguments constants
result = (1073741825 - 2147483649)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1826: LHS constant
y = 2147483649;
result = (1073741825 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1827: RHS constant
x = 1073741825;
result = (x - 2147483649)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1828: both arguments variables
x = 1073741825;
y = -2147483647;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1829: both arguments constants
result = (1073741825 - -2147483647)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1830: LHS constant
y = -2147483647;
result = (1073741825 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1831: RHS constant
x = 1073741825;
result = (x - -2147483647)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1832: both arguments variables
x = 1073741825;
y = -2147483648;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1833: both arguments constants
result = (1073741825 - -2147483648)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1834: LHS constant
y = -2147483648;
result = (1073741825 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1835: RHS constant
x = 1073741825;
result = (x - -2147483648)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1836: both arguments variables
x = 1073741825;
y = -2147483649;
result = (x - y);
check = 3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1837: both arguments constants
result = (1073741825 - -2147483649)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1838: LHS constant
y = -2147483649;
result = (1073741825 - y)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1839: RHS constant
x = 1073741825;
result = (x - -2147483649)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1840: both arguments variables
x = 1073741825;
y = -2147483650;
result = (x - y);
check = 3221225475;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1841: both arguments constants
result = (1073741825 - -2147483650)
check = 3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1842: LHS constant
y = -2147483650;
result = (1073741825 - y)
check = 3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1843: RHS constant
x = 1073741825;
result = (x - -2147483650)
check = 3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1844: both arguments variables
x = 1073741825;
y = 4294967295;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1845: both arguments constants
result = (1073741825 - 4294967295)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1846: LHS constant
y = 4294967295;
result = (1073741825 - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1847: RHS constant
x = 1073741825;
result = (x - 4294967295)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1848: both arguments variables
x = 1073741825;
y = 4294967296;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1849: both arguments constants
result = (1073741825 - 4294967296)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1850: LHS constant
y = 4294967296;
result = (1073741825 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1851: RHS constant
x = 1073741825;
result = (x - 4294967296)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1852: both arguments variables
x = 1073741825;
y = -4294967295;
result = (x - y);
check = 5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1853: both arguments constants
result = (1073741825 - -4294967295)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1854: LHS constant
y = -4294967295;
result = (1073741825 - y)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1855: RHS constant
x = 1073741825;
result = (x - -4294967295)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1856: both arguments variables
x = 1073741825;
y = -4294967296;
result = (x - y);
check = 5368709121;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1857: both arguments constants
result = (1073741825 - -4294967296)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1858: LHS constant
y = -4294967296;
result = (1073741825 - y)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1859: RHS constant
x = 1073741825;
result = (x - -4294967296)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1860: both arguments variables
x = -1073741823;
y = 0;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1861: both arguments constants
result = (-1073741823 - 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1862: LHS constant
y = 0;
result = (-1073741823 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1863: RHS constant
x = -1073741823;
result = (x - 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1864: both arguments variables
x = -1073741823;
y = 1;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1865: both arguments constants
result = (-1073741823 - 1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1866: LHS constant
y = 1;
result = (-1073741823 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1867: RHS constant
x = -1073741823;
result = (x - 1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1868: both arguments variables
x = -1073741823;
y = -1;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1869: both arguments constants
result = (-1073741823 - -1)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1870: LHS constant
y = -1;
result = (-1073741823 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1871: RHS constant
x = -1073741823;
result = (x - -1)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1872: both arguments variables
x = -1073741823;
y = 2;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1873: both arguments constants
result = (-1073741823 - 2)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1874: LHS constant
y = 2;
result = (-1073741823 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1875: RHS constant
x = -1073741823;
result = (x - 2)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1876: both arguments variables
x = -1073741823;
y = -2;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1877: both arguments constants
result = (-1073741823 - -2)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1878: LHS constant
y = -2;
result = (-1073741823 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1879: RHS constant
x = -1073741823;
result = (x - -2)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1880: both arguments variables
x = -1073741823;
y = 3;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1881: both arguments constants
result = (-1073741823 - 3)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1882: LHS constant
y = 3;
result = (-1073741823 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1883: RHS constant
x = -1073741823;
result = (x - 3)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1884: both arguments variables
x = -1073741823;
y = -3;
result = (x - y);
check = -1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1885: both arguments constants
result = (-1073741823 - -3)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1886: LHS constant
y = -3;
result = (-1073741823 - y)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1887: RHS constant
x = -1073741823;
result = (x - -3)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1888: both arguments variables
x = -1073741823;
y = 4;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1889: both arguments constants
result = (-1073741823 - 4)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1890: LHS constant
y = 4;
result = (-1073741823 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1891: RHS constant
x = -1073741823;
result = (x - 4)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1892: both arguments variables
x = -1073741823;
y = -4;
result = (x - y);
check = -1073741819;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1893: both arguments constants
result = (-1073741823 - -4)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1894: LHS constant
y = -4;
result = (-1073741823 - y)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1895: RHS constant
x = -1073741823;
result = (x - -4)
check = -1073741819
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1896: both arguments variables
x = -1073741823;
y = 8;
result = (x - y);
check = -1073741831;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1897: both arguments constants
result = (-1073741823 - 8)
check = -1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1898: LHS constant
y = 8;
result = (-1073741823 - y)
check = -1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1899: RHS constant
x = -1073741823;
result = (x - 8)
check = -1073741831
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test19() {
var x;
var y;
var result;
var check;
// Test 1900: both arguments variables
x = -1073741823;
y = -8;
result = (x - y);
check = -1073741815;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1901: both arguments constants
result = (-1073741823 - -8)
check = -1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1902: LHS constant
y = -8;
result = (-1073741823 - y)
check = -1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1903: RHS constant
x = -1073741823;
result = (x - -8)
check = -1073741815
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1904: both arguments variables
x = -1073741823;
y = 1073741822;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1905: both arguments constants
result = (-1073741823 - 1073741822)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1906: LHS constant
y = 1073741822;
result = (-1073741823 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1907: RHS constant
x = -1073741823;
result = (x - 1073741822)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1908: both arguments variables
x = -1073741823;
y = 1073741823;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1909: both arguments constants
result = (-1073741823 - 1073741823)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1910: LHS constant
y = 1073741823;
result = (-1073741823 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1911: RHS constant
x = -1073741823;
result = (x - 1073741823)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1912: both arguments variables
x = -1073741823;
y = 1073741824;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1913: both arguments constants
result = (-1073741823 - 1073741824)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1914: LHS constant
y = 1073741824;
result = (-1073741823 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1915: RHS constant
x = -1073741823;
result = (x - 1073741824)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1916: both arguments variables
x = -1073741823;
y = 1073741825;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1917: both arguments constants
result = (-1073741823 - 1073741825)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1918: LHS constant
y = 1073741825;
result = (-1073741823 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1919: RHS constant
x = -1073741823;
result = (x - 1073741825)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1920: both arguments variables
x = -1073741823;
y = -1073741823;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1921: both arguments constants
result = (-1073741823 - -1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1922: LHS constant
y = -1073741823;
result = (-1073741823 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1923: RHS constant
x = -1073741823;
result = (x - -1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1924: both arguments variables
x = -1073741823;
y = (-0x3fffffff-1);
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1925: both arguments constants
result = (-1073741823 - (-0x3fffffff-1))
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1926: LHS constant
y = (-0x3fffffff-1);
result = (-1073741823 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1927: RHS constant
x = -1073741823;
result = (x - (-0x3fffffff-1))
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1928: both arguments variables
x = -1073741823;
y = -1073741825;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1929: both arguments constants
result = (-1073741823 - -1073741825)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1930: LHS constant
y = -1073741825;
result = (-1073741823 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1931: RHS constant
x = -1073741823;
result = (x - -1073741825)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1932: both arguments variables
x = -1073741823;
y = -1073741826;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1933: both arguments constants
result = (-1073741823 - -1073741826)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1934: LHS constant
y = -1073741826;
result = (-1073741823 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1935: RHS constant
x = -1073741823;
result = (x - -1073741826)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1936: both arguments variables
x = -1073741823;
y = 2147483646;
result = (x - y);
check = -3221225469;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1937: both arguments constants
result = (-1073741823 - 2147483646)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1938: LHS constant
y = 2147483646;
result = (-1073741823 - y)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1939: RHS constant
x = -1073741823;
result = (x - 2147483646)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1940: both arguments variables
x = -1073741823;
y = 2147483647;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1941: both arguments constants
result = (-1073741823 - 2147483647)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1942: LHS constant
y = 2147483647;
result = (-1073741823 - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1943: RHS constant
x = -1073741823;
result = (x - 2147483647)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1944: both arguments variables
x = -1073741823;
y = 2147483648;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1945: both arguments constants
result = (-1073741823 - 2147483648)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1946: LHS constant
y = 2147483648;
result = (-1073741823 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1947: RHS constant
x = -1073741823;
result = (x - 2147483648)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1948: both arguments variables
x = -1073741823;
y = 2147483649;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1949: both arguments constants
result = (-1073741823 - 2147483649)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1950: LHS constant
y = 2147483649;
result = (-1073741823 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1951: RHS constant
x = -1073741823;
result = (x - 2147483649)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1952: both arguments variables
x = -1073741823;
y = -2147483647;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1953: both arguments constants
result = (-1073741823 - -2147483647)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1954: LHS constant
y = -2147483647;
result = (-1073741823 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1955: RHS constant
x = -1073741823;
result = (x - -2147483647)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1956: both arguments variables
x = -1073741823;
y = -2147483648;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1957: both arguments constants
result = (-1073741823 - -2147483648)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1958: LHS constant
y = -2147483648;
result = (-1073741823 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1959: RHS constant
x = -1073741823;
result = (x - -2147483648)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1960: both arguments variables
x = -1073741823;
y = -2147483649;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1961: both arguments constants
result = (-1073741823 - -2147483649)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1962: LHS constant
y = -2147483649;
result = (-1073741823 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1963: RHS constant
x = -1073741823;
result = (x - -2147483649)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1964: both arguments variables
x = -1073741823;
y = -2147483650;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1965: both arguments constants
result = (-1073741823 - -2147483650)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1966: LHS constant
y = -2147483650;
result = (-1073741823 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1967: RHS constant
x = -1073741823;
result = (x - -2147483650)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1968: both arguments variables
x = -1073741823;
y = 4294967295;
result = (x - y);
check = -5368709118;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1969: both arguments constants
result = (-1073741823 - 4294967295)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1970: LHS constant
y = 4294967295;
result = (-1073741823 - y)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1971: RHS constant
x = -1073741823;
result = (x - 4294967295)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1972: both arguments variables
x = -1073741823;
y = 4294967296;
result = (x - y);
check = -5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1973: both arguments constants
result = (-1073741823 - 4294967296)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1974: LHS constant
y = 4294967296;
result = (-1073741823 - y)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1975: RHS constant
x = -1073741823;
result = (x - 4294967296)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1976: both arguments variables
x = -1073741823;
y = -4294967295;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1977: both arguments constants
result = (-1073741823 - -4294967295)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1978: LHS constant
y = -4294967295;
result = (-1073741823 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1979: RHS constant
x = -1073741823;
result = (x - -4294967295)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1980: both arguments variables
x = -1073741823;
y = -4294967296;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1981: both arguments constants
result = (-1073741823 - -4294967296)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1982: LHS constant
y = -4294967296;
result = (-1073741823 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1983: RHS constant
x = -1073741823;
result = (x - -4294967296)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1984: both arguments variables
x = (-0x3fffffff-1);
y = 0;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1985: both arguments constants
result = ((-0x3fffffff-1) - 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1986: LHS constant
y = 0;
result = ((-0x3fffffff-1) - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1987: RHS constant
x = (-0x3fffffff-1);
result = (x - 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1988: both arguments variables
x = (-0x3fffffff-1);
y = 1;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1989: both arguments constants
result = ((-0x3fffffff-1) - 1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1990: LHS constant
y = 1;
result = ((-0x3fffffff-1) - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1991: RHS constant
x = (-0x3fffffff-1);
result = (x - 1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1992: both arguments variables
x = (-0x3fffffff-1);
y = -1;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1993: both arguments constants
result = ((-0x3fffffff-1) - -1)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1994: LHS constant
y = -1;
result = ((-0x3fffffff-1) - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1995: RHS constant
x = (-0x3fffffff-1);
result = (x - -1)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1996: both arguments variables
x = (-0x3fffffff-1);
y = 2;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1997: both arguments constants
result = ((-0x3fffffff-1) - 2)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1998: LHS constant
y = 2;
result = ((-0x3fffffff-1) - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1999: RHS constant
x = (-0x3fffffff-1);
result = (x - 2)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test20() {
var x;
var y;
var result;
var check;
// Test 2000: both arguments variables
x = (-0x3fffffff-1);
y = -2;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2001: both arguments constants
result = ((-0x3fffffff-1) - -2)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2002: LHS constant
y = -2;
result = ((-0x3fffffff-1) - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2003: RHS constant
x = (-0x3fffffff-1);
result = (x - -2)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2004: both arguments variables
x = (-0x3fffffff-1);
y = 3;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2005: both arguments constants
result = ((-0x3fffffff-1) - 3)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2006: LHS constant
y = 3;
result = ((-0x3fffffff-1) - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2007: RHS constant
x = (-0x3fffffff-1);
result = (x - 3)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2008: both arguments variables
x = (-0x3fffffff-1);
y = -3;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2009: both arguments constants
result = ((-0x3fffffff-1) - -3)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2010: LHS constant
y = -3;
result = ((-0x3fffffff-1) - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2011: RHS constant
x = (-0x3fffffff-1);
result = (x - -3)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2012: both arguments variables
x = (-0x3fffffff-1);
y = 4;
result = (x - y);
check = -1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2013: both arguments constants
result = ((-0x3fffffff-1) - 4)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2014: LHS constant
y = 4;
result = ((-0x3fffffff-1) - y)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2015: RHS constant
x = (-0x3fffffff-1);
result = (x - 4)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2016: both arguments variables
x = (-0x3fffffff-1);
y = -4;
result = (x - y);
check = -1073741820;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2017: both arguments constants
result = ((-0x3fffffff-1) - -4)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2018: LHS constant
y = -4;
result = ((-0x3fffffff-1) - y)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2019: RHS constant
x = (-0x3fffffff-1);
result = (x - -4)
check = -1073741820
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2020: both arguments variables
x = (-0x3fffffff-1);
y = 8;
result = (x - y);
check = -1073741832;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2021: both arguments constants
result = ((-0x3fffffff-1) - 8)
check = -1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2022: LHS constant
y = 8;
result = ((-0x3fffffff-1) - y)
check = -1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2023: RHS constant
x = (-0x3fffffff-1);
result = (x - 8)
check = -1073741832
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2024: both arguments variables
x = (-0x3fffffff-1);
y = -8;
result = (x - y);
check = -1073741816;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2025: both arguments constants
result = ((-0x3fffffff-1) - -8)
check = -1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2026: LHS constant
y = -8;
result = ((-0x3fffffff-1) - y)
check = -1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2027: RHS constant
x = (-0x3fffffff-1);
result = (x - -8)
check = -1073741816
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2028: both arguments variables
x = (-0x3fffffff-1);
y = 1073741822;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2029: both arguments constants
result = ((-0x3fffffff-1) - 1073741822)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2030: LHS constant
y = 1073741822;
result = ((-0x3fffffff-1) - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2031: RHS constant
x = (-0x3fffffff-1);
result = (x - 1073741822)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2032: both arguments variables
x = (-0x3fffffff-1);
y = 1073741823;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2033: both arguments constants
result = ((-0x3fffffff-1) - 1073741823)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2034: LHS constant
y = 1073741823;
result = ((-0x3fffffff-1) - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2035: RHS constant
x = (-0x3fffffff-1);
result = (x - 1073741823)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2036: both arguments variables
x = (-0x3fffffff-1);
y = 1073741824;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2037: both arguments constants
result = ((-0x3fffffff-1) - 1073741824)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2038: LHS constant
y = 1073741824;
result = ((-0x3fffffff-1) - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2039: RHS constant
x = (-0x3fffffff-1);
result = (x - 1073741824)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2040: both arguments variables
x = (-0x3fffffff-1);
y = 1073741825;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2041: both arguments constants
result = ((-0x3fffffff-1) - 1073741825)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2042: LHS constant
y = 1073741825;
result = ((-0x3fffffff-1) - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2043: RHS constant
x = (-0x3fffffff-1);
result = (x - 1073741825)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2044: both arguments variables
x = (-0x3fffffff-1);
y = -1073741823;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2045: both arguments constants
result = ((-0x3fffffff-1) - -1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2046: LHS constant
y = -1073741823;
result = ((-0x3fffffff-1) - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2047: RHS constant
x = (-0x3fffffff-1);
result = (x - -1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2048: both arguments variables
x = (-0x3fffffff-1);
y = (-0x3fffffff-1);
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2049: both arguments constants
result = ((-0x3fffffff-1) - (-0x3fffffff-1))
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2050: LHS constant
y = (-0x3fffffff-1);
result = ((-0x3fffffff-1) - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2051: RHS constant
x = (-0x3fffffff-1);
result = (x - (-0x3fffffff-1))
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2052: both arguments variables
x = (-0x3fffffff-1);
y = -1073741825;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2053: both arguments constants
result = ((-0x3fffffff-1) - -1073741825)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2054: LHS constant
y = -1073741825;
result = ((-0x3fffffff-1) - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2055: RHS constant
x = (-0x3fffffff-1);
result = (x - -1073741825)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2056: both arguments variables
x = (-0x3fffffff-1);
y = -1073741826;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2057: both arguments constants
result = ((-0x3fffffff-1) - -1073741826)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2058: LHS constant
y = -1073741826;
result = ((-0x3fffffff-1) - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2059: RHS constant
x = (-0x3fffffff-1);
result = (x - -1073741826)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2060: both arguments variables
x = (-0x3fffffff-1);
y = 2147483646;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2061: both arguments constants
result = ((-0x3fffffff-1) - 2147483646)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2062: LHS constant
y = 2147483646;
result = ((-0x3fffffff-1) - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2063: RHS constant
x = (-0x3fffffff-1);
result = (x - 2147483646)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2064: both arguments variables
x = (-0x3fffffff-1);
y = 2147483647;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2065: both arguments constants
result = ((-0x3fffffff-1) - 2147483647)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2066: LHS constant
y = 2147483647;
result = ((-0x3fffffff-1) - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2067: RHS constant
x = (-0x3fffffff-1);
result = (x - 2147483647)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2068: both arguments variables
x = (-0x3fffffff-1);
y = 2147483648;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2069: both arguments constants
result = ((-0x3fffffff-1) - 2147483648)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2070: LHS constant
y = 2147483648;
result = ((-0x3fffffff-1) - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2071: RHS constant
x = (-0x3fffffff-1);
result = (x - 2147483648)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2072: both arguments variables
x = (-0x3fffffff-1);
y = 2147483649;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2073: both arguments constants
result = ((-0x3fffffff-1) - 2147483649)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2074: LHS constant
y = 2147483649;
result = ((-0x3fffffff-1) - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2075: RHS constant
x = (-0x3fffffff-1);
result = (x - 2147483649)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2076: both arguments variables
x = (-0x3fffffff-1);
y = -2147483647;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2077: both arguments constants
result = ((-0x3fffffff-1) - -2147483647)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2078: LHS constant
y = -2147483647;
result = ((-0x3fffffff-1) - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2079: RHS constant
x = (-0x3fffffff-1);
result = (x - -2147483647)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2080: both arguments variables
x = (-0x3fffffff-1);
y = -2147483648;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2081: both arguments constants
result = ((-0x3fffffff-1) - -2147483648)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2082: LHS constant
y = -2147483648;
result = ((-0x3fffffff-1) - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2083: RHS constant
x = (-0x3fffffff-1);
result = (x - -2147483648)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2084: both arguments variables
x = (-0x3fffffff-1);
y = -2147483649;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2085: both arguments constants
result = ((-0x3fffffff-1) - -2147483649)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2086: LHS constant
y = -2147483649;
result = ((-0x3fffffff-1) - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2087: RHS constant
x = (-0x3fffffff-1);
result = (x - -2147483649)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2088: both arguments variables
x = (-0x3fffffff-1);
y = -2147483650;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2089: both arguments constants
result = ((-0x3fffffff-1) - -2147483650)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2090: LHS constant
y = -2147483650;
result = ((-0x3fffffff-1) - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2091: RHS constant
x = (-0x3fffffff-1);
result = (x - -2147483650)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2092: both arguments variables
x = (-0x3fffffff-1);
y = 4294967295;
result = (x - y);
check = -5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2093: both arguments constants
result = ((-0x3fffffff-1) - 4294967295)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2094: LHS constant
y = 4294967295;
result = ((-0x3fffffff-1) - y)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2095: RHS constant
x = (-0x3fffffff-1);
result = (x - 4294967295)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2096: both arguments variables
x = (-0x3fffffff-1);
y = 4294967296;
result = (x - y);
check = -5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2097: both arguments constants
result = ((-0x3fffffff-1) - 4294967296)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2098: LHS constant
y = 4294967296;
result = ((-0x3fffffff-1) - y)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2099: RHS constant
x = (-0x3fffffff-1);
result = (x - 4294967296)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test21() {
var x;
var y;
var result;
var check;
// Test 2100: both arguments variables
x = (-0x3fffffff-1);
y = -4294967295;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2101: both arguments constants
result = ((-0x3fffffff-1) - -4294967295)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2102: LHS constant
y = -4294967295;
result = ((-0x3fffffff-1) - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2103: RHS constant
x = (-0x3fffffff-1);
result = (x - -4294967295)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2104: both arguments variables
x = (-0x3fffffff-1);
y = -4294967296;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2105: both arguments constants
result = ((-0x3fffffff-1) - -4294967296)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2106: LHS constant
y = -4294967296;
result = ((-0x3fffffff-1) - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2107: RHS constant
x = (-0x3fffffff-1);
result = (x - -4294967296)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2108: both arguments variables
x = -1073741825;
y = 0;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2109: both arguments constants
result = (-1073741825 - 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2110: LHS constant
y = 0;
result = (-1073741825 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2111: RHS constant
x = -1073741825;
result = (x - 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2112: both arguments variables
x = -1073741825;
y = 1;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2113: both arguments constants
result = (-1073741825 - 1)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2114: LHS constant
y = 1;
result = (-1073741825 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2115: RHS constant
x = -1073741825;
result = (x - 1)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2116: both arguments variables
x = -1073741825;
y = -1;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2117: both arguments constants
result = (-1073741825 - -1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2118: LHS constant
y = -1;
result = (-1073741825 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2119: RHS constant
x = -1073741825;
result = (x - -1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2120: both arguments variables
x = -1073741825;
y = 2;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2121: both arguments constants
result = (-1073741825 - 2)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2122: LHS constant
y = 2;
result = (-1073741825 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2123: RHS constant
x = -1073741825;
result = (x - 2)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2124: both arguments variables
x = -1073741825;
y = -2;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2125: both arguments constants
result = (-1073741825 - -2)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2126: LHS constant
y = -2;
result = (-1073741825 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2127: RHS constant
x = -1073741825;
result = (x - -2)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2128: both arguments variables
x = -1073741825;
y = 3;
result = (x - y);
check = -1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2129: both arguments constants
result = (-1073741825 - 3)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2130: LHS constant
y = 3;
result = (-1073741825 - y)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2131: RHS constant
x = -1073741825;
result = (x - 3)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2132: both arguments variables
x = -1073741825;
y = -3;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2133: both arguments constants
result = (-1073741825 - -3)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2134: LHS constant
y = -3;
result = (-1073741825 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2135: RHS constant
x = -1073741825;
result = (x - -3)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2136: both arguments variables
x = -1073741825;
y = 4;
result = (x - y);
check = -1073741829;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2137: both arguments constants
result = (-1073741825 - 4)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2138: LHS constant
y = 4;
result = (-1073741825 - y)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2139: RHS constant
x = -1073741825;
result = (x - 4)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2140: both arguments variables
x = -1073741825;
y = -4;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2141: both arguments constants
result = (-1073741825 - -4)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2142: LHS constant
y = -4;
result = (-1073741825 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2143: RHS constant
x = -1073741825;
result = (x - -4)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2144: both arguments variables
x = -1073741825;
y = 8;
result = (x - y);
check = -1073741833;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2145: both arguments constants
result = (-1073741825 - 8)
check = -1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2146: LHS constant
y = 8;
result = (-1073741825 - y)
check = -1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2147: RHS constant
x = -1073741825;
result = (x - 8)
check = -1073741833
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2148: both arguments variables
x = -1073741825;
y = -8;
result = (x - y);
check = -1073741817;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2149: both arguments constants
result = (-1073741825 - -8)
check = -1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2150: LHS constant
y = -8;
result = (-1073741825 - y)
check = -1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2151: RHS constant
x = -1073741825;
result = (x - -8)
check = -1073741817
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2152: both arguments variables
x = -1073741825;
y = 1073741822;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2153: both arguments constants
result = (-1073741825 - 1073741822)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2154: LHS constant
y = 1073741822;
result = (-1073741825 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2155: RHS constant
x = -1073741825;
result = (x - 1073741822)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2156: both arguments variables
x = -1073741825;
y = 1073741823;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2157: both arguments constants
result = (-1073741825 - 1073741823)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2158: LHS constant
y = 1073741823;
result = (-1073741825 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2159: RHS constant
x = -1073741825;
result = (x - 1073741823)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2160: both arguments variables
x = -1073741825;
y = 1073741824;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2161: both arguments constants
result = (-1073741825 - 1073741824)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2162: LHS constant
y = 1073741824;
result = (-1073741825 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2163: RHS constant
x = -1073741825;
result = (x - 1073741824)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2164: both arguments variables
x = -1073741825;
y = 1073741825;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2165: both arguments constants
result = (-1073741825 - 1073741825)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2166: LHS constant
y = 1073741825;
result = (-1073741825 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2167: RHS constant
x = -1073741825;
result = (x - 1073741825)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2168: both arguments variables
x = -1073741825;
y = -1073741823;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2169: both arguments constants
result = (-1073741825 - -1073741823)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2170: LHS constant
y = -1073741823;
result = (-1073741825 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2171: RHS constant
x = -1073741825;
result = (x - -1073741823)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2172: both arguments variables
x = -1073741825;
y = (-0x3fffffff-1);
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2173: both arguments constants
result = (-1073741825 - (-0x3fffffff-1))
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2174: LHS constant
y = (-0x3fffffff-1);
result = (-1073741825 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2175: RHS constant
x = -1073741825;
result = (x - (-0x3fffffff-1))
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2176: both arguments variables
x = -1073741825;
y = -1073741825;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2177: both arguments constants
result = (-1073741825 - -1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2178: LHS constant
y = -1073741825;
result = (-1073741825 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2179: RHS constant
x = -1073741825;
result = (x - -1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2180: both arguments variables
x = -1073741825;
y = -1073741826;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2181: both arguments constants
result = (-1073741825 - -1073741826)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2182: LHS constant
y = -1073741826;
result = (-1073741825 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2183: RHS constant
x = -1073741825;
result = (x - -1073741826)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2184: both arguments variables
x = -1073741825;
y = 2147483646;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2185: both arguments constants
result = (-1073741825 - 2147483646)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2186: LHS constant
y = 2147483646;
result = (-1073741825 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2187: RHS constant
x = -1073741825;
result = (x - 2147483646)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2188: both arguments variables
x = -1073741825;
y = 2147483647;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2189: both arguments constants
result = (-1073741825 - 2147483647)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2190: LHS constant
y = 2147483647;
result = (-1073741825 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2191: RHS constant
x = -1073741825;
result = (x - 2147483647)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2192: both arguments variables
x = -1073741825;
y = 2147483648;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2193: both arguments constants
result = (-1073741825 - 2147483648)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2194: LHS constant
y = 2147483648;
result = (-1073741825 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2195: RHS constant
x = -1073741825;
result = (x - 2147483648)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2196: both arguments variables
x = -1073741825;
y = 2147483649;
result = (x - y);
check = -3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2197: both arguments constants
result = (-1073741825 - 2147483649)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2198: LHS constant
y = 2147483649;
result = (-1073741825 - y)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2199: RHS constant
x = -1073741825;
result = (x - 2147483649)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test22() {
var x;
var y;
var result;
var check;
// Test 2200: both arguments variables
x = -1073741825;
y = -2147483647;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2201: both arguments constants
result = (-1073741825 - -2147483647)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2202: LHS constant
y = -2147483647;
result = (-1073741825 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2203: RHS constant
x = -1073741825;
result = (x - -2147483647)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2204: both arguments variables
x = -1073741825;
y = -2147483648;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2205: both arguments constants
result = (-1073741825 - -2147483648)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2206: LHS constant
y = -2147483648;
result = (-1073741825 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2207: RHS constant
x = -1073741825;
result = (x - -2147483648)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2208: both arguments variables
x = -1073741825;
y = -2147483649;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2209: both arguments constants
result = (-1073741825 - -2147483649)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2210: LHS constant
y = -2147483649;
result = (-1073741825 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2211: RHS constant
x = -1073741825;
result = (x - -2147483649)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2212: both arguments variables
x = -1073741825;
y = -2147483650;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2213: both arguments constants
result = (-1073741825 - -2147483650)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2214: LHS constant
y = -2147483650;
result = (-1073741825 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2215: RHS constant
x = -1073741825;
result = (x - -2147483650)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2216: both arguments variables
x = -1073741825;
y = 4294967295;
result = (x - y);
check = -5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2217: both arguments constants
result = (-1073741825 - 4294967295)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2218: LHS constant
y = 4294967295;
result = (-1073741825 - y)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2219: RHS constant
x = -1073741825;
result = (x - 4294967295)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2220: both arguments variables
x = -1073741825;
y = 4294967296;
result = (x - y);
check = -5368709121;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2221: both arguments constants
result = (-1073741825 - 4294967296)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2222: LHS constant
y = 4294967296;
result = (-1073741825 - y)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2223: RHS constant
x = -1073741825;
result = (x - 4294967296)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2224: both arguments variables
x = -1073741825;
y = -4294967295;
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2225: both arguments constants
result = (-1073741825 - -4294967295)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2226: LHS constant
y = -4294967295;
result = (-1073741825 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2227: RHS constant
x = -1073741825;
result = (x - -4294967295)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2228: both arguments variables
x = -1073741825;
y = -4294967296;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2229: both arguments constants
result = (-1073741825 - -4294967296)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2230: LHS constant
y = -4294967296;
result = (-1073741825 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2231: RHS constant
x = -1073741825;
result = (x - -4294967296)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2232: both arguments variables
x = -1073741826;
y = 0;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2233: both arguments constants
result = (-1073741826 - 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2234: LHS constant
y = 0;
result = (-1073741826 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2235: RHS constant
x = -1073741826;
result = (x - 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2236: both arguments variables
x = -1073741826;
y = 1;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2237: both arguments constants
result = (-1073741826 - 1)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2238: LHS constant
y = 1;
result = (-1073741826 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2239: RHS constant
x = -1073741826;
result = (x - 1)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2240: both arguments variables
x = -1073741826;
y = -1;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2241: both arguments constants
result = (-1073741826 - -1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2242: LHS constant
y = -1;
result = (-1073741826 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2243: RHS constant
x = -1073741826;
result = (x - -1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2244: both arguments variables
x = -1073741826;
y = 2;
result = (x - y);
check = -1073741828;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2245: both arguments constants
result = (-1073741826 - 2)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2246: LHS constant
y = 2;
result = (-1073741826 - y)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2247: RHS constant
x = -1073741826;
result = (x - 2)
check = -1073741828
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2248: both arguments variables
x = -1073741826;
y = -2;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2249: both arguments constants
result = (-1073741826 - -2)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2250: LHS constant
y = -2;
result = (-1073741826 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2251: RHS constant
x = -1073741826;
result = (x - -2)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2252: both arguments variables
x = -1073741826;
y = 3;
result = (x - y);
check = -1073741829;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2253: both arguments constants
result = (-1073741826 - 3)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2254: LHS constant
y = 3;
result = (-1073741826 - y)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2255: RHS constant
x = -1073741826;
result = (x - 3)
check = -1073741829
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2256: both arguments variables
x = -1073741826;
y = -3;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2257: both arguments constants
result = (-1073741826 - -3)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2258: LHS constant
y = -3;
result = (-1073741826 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2259: RHS constant
x = -1073741826;
result = (x - -3)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2260: both arguments variables
x = -1073741826;
y = 4;
result = (x - y);
check = -1073741830;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2261: both arguments constants
result = (-1073741826 - 4)
check = -1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2262: LHS constant
y = 4;
result = (-1073741826 - y)
check = -1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2263: RHS constant
x = -1073741826;
result = (x - 4)
check = -1073741830
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2264: both arguments variables
x = -1073741826;
y = -4;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2265: both arguments constants
result = (-1073741826 - -4)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2266: LHS constant
y = -4;
result = (-1073741826 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2267: RHS constant
x = -1073741826;
result = (x - -4)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2268: both arguments variables
x = -1073741826;
y = 8;
result = (x - y);
check = -1073741834;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2269: both arguments constants
result = (-1073741826 - 8)
check = -1073741834
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2270: LHS constant
y = 8;
result = (-1073741826 - y)
check = -1073741834
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2271: RHS constant
x = -1073741826;
result = (x - 8)
check = -1073741834
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2272: both arguments variables
x = -1073741826;
y = -8;
result = (x - y);
check = -1073741818;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2273: both arguments constants
result = (-1073741826 - -8)
check = -1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2274: LHS constant
y = -8;
result = (-1073741826 - y)
check = -1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2275: RHS constant
x = -1073741826;
result = (x - -8)
check = -1073741818
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2276: both arguments variables
x = -1073741826;
y = 1073741822;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2277: both arguments constants
result = (-1073741826 - 1073741822)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2278: LHS constant
y = 1073741822;
result = (-1073741826 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2279: RHS constant
x = -1073741826;
result = (x - 1073741822)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2280: both arguments variables
x = -1073741826;
y = 1073741823;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2281: both arguments constants
result = (-1073741826 - 1073741823)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2282: LHS constant
y = 1073741823;
result = (-1073741826 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2283: RHS constant
x = -1073741826;
result = (x - 1073741823)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2284: both arguments variables
x = -1073741826;
y = 1073741824;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2285: both arguments constants
result = (-1073741826 - 1073741824)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2286: LHS constant
y = 1073741824;
result = (-1073741826 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2287: RHS constant
x = -1073741826;
result = (x - 1073741824)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2288: both arguments variables
x = -1073741826;
y = 1073741825;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2289: both arguments constants
result = (-1073741826 - 1073741825)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2290: LHS constant
y = 1073741825;
result = (-1073741826 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2291: RHS constant
x = -1073741826;
result = (x - 1073741825)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2292: both arguments variables
x = -1073741826;
y = -1073741823;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2293: both arguments constants
result = (-1073741826 - -1073741823)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2294: LHS constant
y = -1073741823;
result = (-1073741826 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2295: RHS constant
x = -1073741826;
result = (x - -1073741823)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2296: both arguments variables
x = -1073741826;
y = (-0x3fffffff-1);
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2297: both arguments constants
result = (-1073741826 - (-0x3fffffff-1))
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2298: LHS constant
y = (-0x3fffffff-1);
result = (-1073741826 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2299: RHS constant
x = -1073741826;
result = (x - (-0x3fffffff-1))
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test23() {
var x;
var y;
var result;
var check;
// Test 2300: both arguments variables
x = -1073741826;
y = -1073741825;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2301: both arguments constants
result = (-1073741826 - -1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2302: LHS constant
y = -1073741825;
result = (-1073741826 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2303: RHS constant
x = -1073741826;
result = (x - -1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2304: both arguments variables
x = -1073741826;
y = -1073741826;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2305: both arguments constants
result = (-1073741826 - -1073741826)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2306: LHS constant
y = -1073741826;
result = (-1073741826 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2307: RHS constant
x = -1073741826;
result = (x - -1073741826)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2308: both arguments variables
x = -1073741826;
y = 2147483646;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2309: both arguments constants
result = (-1073741826 - 2147483646)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2310: LHS constant
y = 2147483646;
result = (-1073741826 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2311: RHS constant
x = -1073741826;
result = (x - 2147483646)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2312: both arguments variables
x = -1073741826;
y = 2147483647;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2313: both arguments constants
result = (-1073741826 - 2147483647)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2314: LHS constant
y = 2147483647;
result = (-1073741826 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2315: RHS constant
x = -1073741826;
result = (x - 2147483647)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2316: both arguments variables
x = -1073741826;
y = 2147483648;
result = (x - y);
check = -3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2317: both arguments constants
result = (-1073741826 - 2147483648)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2318: LHS constant
y = 2147483648;
result = (-1073741826 - y)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2319: RHS constant
x = -1073741826;
result = (x - 2147483648)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2320: both arguments variables
x = -1073741826;
y = 2147483649;
result = (x - y);
check = -3221225475;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2321: both arguments constants
result = (-1073741826 - 2147483649)
check = -3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2322: LHS constant
y = 2147483649;
result = (-1073741826 - y)
check = -3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2323: RHS constant
x = -1073741826;
result = (x - 2147483649)
check = -3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2324: both arguments variables
x = -1073741826;
y = -2147483647;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2325: both arguments constants
result = (-1073741826 - -2147483647)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2326: LHS constant
y = -2147483647;
result = (-1073741826 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2327: RHS constant
x = -1073741826;
result = (x - -2147483647)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2328: both arguments variables
x = -1073741826;
y = -2147483648;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2329: both arguments constants
result = (-1073741826 - -2147483648)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2330: LHS constant
y = -2147483648;
result = (-1073741826 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2331: RHS constant
x = -1073741826;
result = (x - -2147483648)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2332: both arguments variables
x = -1073741826;
y = -2147483649;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2333: both arguments constants
result = (-1073741826 - -2147483649)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2334: LHS constant
y = -2147483649;
result = (-1073741826 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2335: RHS constant
x = -1073741826;
result = (x - -2147483649)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2336: both arguments variables
x = -1073741826;
y = -2147483650;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2337: both arguments constants
result = (-1073741826 - -2147483650)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2338: LHS constant
y = -2147483650;
result = (-1073741826 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2339: RHS constant
x = -1073741826;
result = (x - -2147483650)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2340: both arguments variables
x = -1073741826;
y = 4294967295;
result = (x - y);
check = -5368709121;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2341: both arguments constants
result = (-1073741826 - 4294967295)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2342: LHS constant
y = 4294967295;
result = (-1073741826 - y)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2343: RHS constant
x = -1073741826;
result = (x - 4294967295)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2344: both arguments variables
x = -1073741826;
y = 4294967296;
result = (x - y);
check = -5368709122;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2345: both arguments constants
result = (-1073741826 - 4294967296)
check = -5368709122
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2346: LHS constant
y = 4294967296;
result = (-1073741826 - y)
check = -5368709122
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2347: RHS constant
x = -1073741826;
result = (x - 4294967296)
check = -5368709122
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2348: both arguments variables
x = -1073741826;
y = -4294967295;
result = (x - y);
check = 3221225469;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2349: both arguments constants
result = (-1073741826 - -4294967295)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2350: LHS constant
y = -4294967295;
result = (-1073741826 - y)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2351: RHS constant
x = -1073741826;
result = (x - -4294967295)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2352: both arguments variables
x = -1073741826;
y = -4294967296;
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2353: both arguments constants
result = (-1073741826 - -4294967296)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2354: LHS constant
y = -4294967296;
result = (-1073741826 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2355: RHS constant
x = -1073741826;
result = (x - -4294967296)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2356: both arguments variables
x = 2147483646;
y = 0;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2357: both arguments constants
result = (2147483646 - 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2358: LHS constant
y = 0;
result = (2147483646 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2359: RHS constant
x = 2147483646;
result = (x - 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2360: both arguments variables
x = 2147483646;
y = 1;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2361: both arguments constants
result = (2147483646 - 1)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2362: LHS constant
y = 1;
result = (2147483646 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2363: RHS constant
x = 2147483646;
result = (x - 1)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2364: both arguments variables
x = 2147483646;
y = -1;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2365: both arguments constants
result = (2147483646 - -1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2366: LHS constant
y = -1;
result = (2147483646 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2367: RHS constant
x = 2147483646;
result = (x - -1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2368: both arguments variables
x = 2147483646;
y = 2;
result = (x - y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2369: both arguments constants
result = (2147483646 - 2)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2370: LHS constant
y = 2;
result = (2147483646 - y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2371: RHS constant
x = 2147483646;
result = (x - 2)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2372: both arguments variables
x = 2147483646;
y = -2;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2373: both arguments constants
result = (2147483646 - -2)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2374: LHS constant
y = -2;
result = (2147483646 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2375: RHS constant
x = 2147483646;
result = (x - -2)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2376: both arguments variables
x = 2147483646;
y = 3;
result = (x - y);
check = 2147483643;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2377: both arguments constants
result = (2147483646 - 3)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2378: LHS constant
y = 3;
result = (2147483646 - y)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2379: RHS constant
x = 2147483646;
result = (x - 3)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2380: both arguments variables
x = 2147483646;
y = -3;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2381: both arguments constants
result = (2147483646 - -3)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2382: LHS constant
y = -3;
result = (2147483646 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2383: RHS constant
x = 2147483646;
result = (x - -3)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2384: both arguments variables
x = 2147483646;
y = 4;
result = (x - y);
check = 2147483642;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2385: both arguments constants
result = (2147483646 - 4)
check = 2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2386: LHS constant
y = 4;
result = (2147483646 - y)
check = 2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2387: RHS constant
x = 2147483646;
result = (x - 4)
check = 2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2388: both arguments variables
x = 2147483646;
y = -4;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2389: both arguments constants
result = (2147483646 - -4)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2390: LHS constant
y = -4;
result = (2147483646 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2391: RHS constant
x = 2147483646;
result = (x - -4)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2392: both arguments variables
x = 2147483646;
y = 8;
result = (x - y);
check = 2147483638;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2393: both arguments constants
result = (2147483646 - 8)
check = 2147483638
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2394: LHS constant
y = 8;
result = (2147483646 - y)
check = 2147483638
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2395: RHS constant
x = 2147483646;
result = (x - 8)
check = 2147483638
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2396: both arguments variables
x = 2147483646;
y = -8;
result = (x - y);
check = 2147483654;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2397: both arguments constants
result = (2147483646 - -8)
check = 2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2398: LHS constant
y = -8;
result = (2147483646 - y)
check = 2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2399: RHS constant
x = 2147483646;
result = (x - -8)
check = 2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test24() {
var x;
var y;
var result;
var check;
// Test 2400: both arguments variables
x = 2147483646;
y = 1073741822;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2401: both arguments constants
result = (2147483646 - 1073741822)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2402: LHS constant
y = 1073741822;
result = (2147483646 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2403: RHS constant
x = 2147483646;
result = (x - 1073741822)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2404: both arguments variables
x = 2147483646;
y = 1073741823;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2405: both arguments constants
result = (2147483646 - 1073741823)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2406: LHS constant
y = 1073741823;
result = (2147483646 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2407: RHS constant
x = 2147483646;
result = (x - 1073741823)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2408: both arguments variables
x = 2147483646;
y = 1073741824;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2409: both arguments constants
result = (2147483646 - 1073741824)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2410: LHS constant
y = 1073741824;
result = (2147483646 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2411: RHS constant
x = 2147483646;
result = (x - 1073741824)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2412: both arguments variables
x = 2147483646;
y = 1073741825;
result = (x - y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2413: both arguments constants
result = (2147483646 - 1073741825)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2414: LHS constant
y = 1073741825;
result = (2147483646 - y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2415: RHS constant
x = 2147483646;
result = (x - 1073741825)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2416: both arguments variables
x = 2147483646;
y = -1073741823;
result = (x - y);
check = 3221225469;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2417: both arguments constants
result = (2147483646 - -1073741823)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2418: LHS constant
y = -1073741823;
result = (2147483646 - y)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2419: RHS constant
x = 2147483646;
result = (x - -1073741823)
check = 3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2420: both arguments variables
x = 2147483646;
y = (-0x3fffffff-1);
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2421: both arguments constants
result = (2147483646 - (-0x3fffffff-1))
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2422: LHS constant
y = (-0x3fffffff-1);
result = (2147483646 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2423: RHS constant
x = 2147483646;
result = (x - (-0x3fffffff-1))
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2424: both arguments variables
x = 2147483646;
y = -1073741825;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2425: both arguments constants
result = (2147483646 - -1073741825)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2426: LHS constant
y = -1073741825;
result = (2147483646 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2427: RHS constant
x = 2147483646;
result = (x - -1073741825)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2428: both arguments variables
x = 2147483646;
y = -1073741826;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2429: both arguments constants
result = (2147483646 - -1073741826)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2430: LHS constant
y = -1073741826;
result = (2147483646 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2431: RHS constant
x = 2147483646;
result = (x - -1073741826)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2432: both arguments variables
x = 2147483646;
y = 2147483646;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2433: both arguments constants
result = (2147483646 - 2147483646)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2434: LHS constant
y = 2147483646;
result = (2147483646 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2435: RHS constant
x = 2147483646;
result = (x - 2147483646)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2436: both arguments variables
x = 2147483646;
y = 2147483647;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2437: both arguments constants
result = (2147483646 - 2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2438: LHS constant
y = 2147483647;
result = (2147483646 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2439: RHS constant
x = 2147483646;
result = (x - 2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2440: both arguments variables
x = 2147483646;
y = 2147483648;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2441: both arguments constants
result = (2147483646 - 2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2442: LHS constant
y = 2147483648;
result = (2147483646 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2443: RHS constant
x = 2147483646;
result = (x - 2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2444: both arguments variables
x = 2147483646;
y = 2147483649;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2445: both arguments constants
result = (2147483646 - 2147483649)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2446: LHS constant
y = 2147483649;
result = (2147483646 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2447: RHS constant
x = 2147483646;
result = (x - 2147483649)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2448: both arguments variables
x = 2147483646;
y = -2147483647;
result = (x - y);
check = 4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2449: both arguments constants
result = (2147483646 - -2147483647)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2450: LHS constant
y = -2147483647;
result = (2147483646 - y)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2451: RHS constant
x = 2147483646;
result = (x - -2147483647)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2452: both arguments variables
x = 2147483646;
y = -2147483648;
result = (x - y);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2453: both arguments constants
result = (2147483646 - -2147483648)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2454: LHS constant
y = -2147483648;
result = (2147483646 - y)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2455: RHS constant
x = 2147483646;
result = (x - -2147483648)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2456: both arguments variables
x = 2147483646;
y = -2147483649;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2457: both arguments constants
result = (2147483646 - -2147483649)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2458: LHS constant
y = -2147483649;
result = (2147483646 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2459: RHS constant
x = 2147483646;
result = (x - -2147483649)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2460: both arguments variables
x = 2147483646;
y = -2147483650;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2461: both arguments constants
result = (2147483646 - -2147483650)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2462: LHS constant
y = -2147483650;
result = (2147483646 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2463: RHS constant
x = 2147483646;
result = (x - -2147483650)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2464: both arguments variables
x = 2147483646;
y = 4294967295;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2465: both arguments constants
result = (2147483646 - 4294967295)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2466: LHS constant
y = 4294967295;
result = (2147483646 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2467: RHS constant
x = 2147483646;
result = (x - 4294967295)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2468: both arguments variables
x = 2147483646;
y = 4294967296;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2469: both arguments constants
result = (2147483646 - 4294967296)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2470: LHS constant
y = 4294967296;
result = (2147483646 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2471: RHS constant
x = 2147483646;
result = (x - 4294967296)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2472: both arguments variables
x = 2147483646;
y = -4294967295;
result = (x - y);
check = 6442450941;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2473: both arguments constants
result = (2147483646 - -4294967295)
check = 6442450941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2474: LHS constant
y = -4294967295;
result = (2147483646 - y)
check = 6442450941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2475: RHS constant
x = 2147483646;
result = (x - -4294967295)
check = 6442450941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2476: both arguments variables
x = 2147483646;
y = -4294967296;
result = (x - y);
check = 6442450942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2477: both arguments constants
result = (2147483646 - -4294967296)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2478: LHS constant
y = -4294967296;
result = (2147483646 - y)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2479: RHS constant
x = 2147483646;
result = (x - -4294967296)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2480: both arguments variables
x = 2147483647;
y = 0;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2481: both arguments constants
result = (2147483647 - 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2482: LHS constant
y = 0;
result = (2147483647 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2483: RHS constant
x = 2147483647;
result = (x - 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2484: both arguments variables
x = 2147483647;
y = 1;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2485: both arguments constants
result = (2147483647 - 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2486: LHS constant
y = 1;
result = (2147483647 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2487: RHS constant
x = 2147483647;
result = (x - 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2488: both arguments variables
x = 2147483647;
y = -1;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2489: both arguments constants
result = (2147483647 - -1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2490: LHS constant
y = -1;
result = (2147483647 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2491: RHS constant
x = 2147483647;
result = (x - -1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2492: both arguments variables
x = 2147483647;
y = 2;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2493: both arguments constants
result = (2147483647 - 2)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2494: LHS constant
y = 2;
result = (2147483647 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2495: RHS constant
x = 2147483647;
result = (x - 2)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2496: both arguments variables
x = 2147483647;
y = -2;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2497: both arguments constants
result = (2147483647 - -2)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2498: LHS constant
y = -2;
result = (2147483647 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2499: RHS constant
x = 2147483647;
result = (x - -2)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test25() {
var x;
var y;
var result;
var check;
// Test 2500: both arguments variables
x = 2147483647;
y = 3;
result = (x - y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2501: both arguments constants
result = (2147483647 - 3)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2502: LHS constant
y = 3;
result = (2147483647 - y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2503: RHS constant
x = 2147483647;
result = (x - 3)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2504: both arguments variables
x = 2147483647;
y = -3;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2505: both arguments constants
result = (2147483647 - -3)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2506: LHS constant
y = -3;
result = (2147483647 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2507: RHS constant
x = 2147483647;
result = (x - -3)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2508: both arguments variables
x = 2147483647;
y = 4;
result = (x - y);
check = 2147483643;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2509: both arguments constants
result = (2147483647 - 4)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2510: LHS constant
y = 4;
result = (2147483647 - y)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2511: RHS constant
x = 2147483647;
result = (x - 4)
check = 2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2512: both arguments variables
x = 2147483647;
y = -4;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2513: both arguments constants
result = (2147483647 - -4)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2514: LHS constant
y = -4;
result = (2147483647 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2515: RHS constant
x = 2147483647;
result = (x - -4)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2516: both arguments variables
x = 2147483647;
y = 8;
result = (x - y);
check = 2147483639;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2517: both arguments constants
result = (2147483647 - 8)
check = 2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2518: LHS constant
y = 8;
result = (2147483647 - y)
check = 2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2519: RHS constant
x = 2147483647;
result = (x - 8)
check = 2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2520: both arguments variables
x = 2147483647;
y = -8;
result = (x - y);
check = 2147483655;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2521: both arguments constants
result = (2147483647 - -8)
check = 2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2522: LHS constant
y = -8;
result = (2147483647 - y)
check = 2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2523: RHS constant
x = 2147483647;
result = (x - -8)
check = 2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2524: both arguments variables
x = 2147483647;
y = 1073741822;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2525: both arguments constants
result = (2147483647 - 1073741822)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2526: LHS constant
y = 1073741822;
result = (2147483647 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2527: RHS constant
x = 2147483647;
result = (x - 1073741822)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2528: both arguments variables
x = 2147483647;
y = 1073741823;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2529: both arguments constants
result = (2147483647 - 1073741823)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2530: LHS constant
y = 1073741823;
result = (2147483647 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2531: RHS constant
x = 2147483647;
result = (x - 1073741823)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2532: both arguments variables
x = 2147483647;
y = 1073741824;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2533: both arguments constants
result = (2147483647 - 1073741824)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2534: LHS constant
y = 1073741824;
result = (2147483647 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2535: RHS constant
x = 2147483647;
result = (x - 1073741824)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2536: both arguments variables
x = 2147483647;
y = 1073741825;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2537: both arguments constants
result = (2147483647 - 1073741825)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2538: LHS constant
y = 1073741825;
result = (2147483647 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2539: RHS constant
x = 2147483647;
result = (x - 1073741825)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2540: both arguments variables
x = 2147483647;
y = -1073741823;
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2541: both arguments constants
result = (2147483647 - -1073741823)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2542: LHS constant
y = -1073741823;
result = (2147483647 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2543: RHS constant
x = 2147483647;
result = (x - -1073741823)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2544: both arguments variables
x = 2147483647;
y = (-0x3fffffff-1);
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2545: both arguments constants
result = (2147483647 - (-0x3fffffff-1))
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2546: LHS constant
y = (-0x3fffffff-1);
result = (2147483647 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2547: RHS constant
x = 2147483647;
result = (x - (-0x3fffffff-1))
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2548: both arguments variables
x = 2147483647;
y = -1073741825;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2549: both arguments constants
result = (2147483647 - -1073741825)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2550: LHS constant
y = -1073741825;
result = (2147483647 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2551: RHS constant
x = 2147483647;
result = (x - -1073741825)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2552: both arguments variables
x = 2147483647;
y = -1073741826;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2553: both arguments constants
result = (2147483647 - -1073741826)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2554: LHS constant
y = -1073741826;
result = (2147483647 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2555: RHS constant
x = 2147483647;
result = (x - -1073741826)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2556: both arguments variables
x = 2147483647;
y = 2147483646;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2557: both arguments constants
result = (2147483647 - 2147483646)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2558: LHS constant
y = 2147483646;
result = (2147483647 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2559: RHS constant
x = 2147483647;
result = (x - 2147483646)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2560: both arguments variables
x = 2147483647;
y = 2147483647;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2561: both arguments constants
result = (2147483647 - 2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2562: LHS constant
y = 2147483647;
result = (2147483647 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2563: RHS constant
x = 2147483647;
result = (x - 2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2564: both arguments variables
x = 2147483647;
y = 2147483648;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2565: both arguments constants
result = (2147483647 - 2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2566: LHS constant
y = 2147483648;
result = (2147483647 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2567: RHS constant
x = 2147483647;
result = (x - 2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2568: both arguments variables
x = 2147483647;
y = 2147483649;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2569: both arguments constants
result = (2147483647 - 2147483649)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2570: LHS constant
y = 2147483649;
result = (2147483647 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2571: RHS constant
x = 2147483647;
result = (x - 2147483649)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2572: both arguments variables
x = 2147483647;
y = -2147483647;
result = (x - y);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2573: both arguments constants
result = (2147483647 - -2147483647)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2574: LHS constant
y = -2147483647;
result = (2147483647 - y)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2575: RHS constant
x = 2147483647;
result = (x - -2147483647)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2576: both arguments variables
x = 2147483647;
y = -2147483648;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2577: both arguments constants
result = (2147483647 - -2147483648)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2578: LHS constant
y = -2147483648;
result = (2147483647 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2579: RHS constant
x = 2147483647;
result = (x - -2147483648)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2580: both arguments variables
x = 2147483647;
y = -2147483649;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2581: both arguments constants
result = (2147483647 - -2147483649)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2582: LHS constant
y = -2147483649;
result = (2147483647 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2583: RHS constant
x = 2147483647;
result = (x - -2147483649)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2584: both arguments variables
x = 2147483647;
y = -2147483650;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2585: both arguments constants
result = (2147483647 - -2147483650)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2586: LHS constant
y = -2147483650;
result = (2147483647 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2587: RHS constant
x = 2147483647;
result = (x - -2147483650)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2588: both arguments variables
x = 2147483647;
y = 4294967295;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2589: both arguments constants
result = (2147483647 - 4294967295)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2590: LHS constant
y = 4294967295;
result = (2147483647 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2591: RHS constant
x = 2147483647;
result = (x - 4294967295)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2592: both arguments variables
x = 2147483647;
y = 4294967296;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2593: both arguments constants
result = (2147483647 - 4294967296)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2594: LHS constant
y = 4294967296;
result = (2147483647 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2595: RHS constant
x = 2147483647;
result = (x - 4294967296)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2596: both arguments variables
x = 2147483647;
y = -4294967295;
result = (x - y);
check = 6442450942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2597: both arguments constants
result = (2147483647 - -4294967295)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2598: LHS constant
y = -4294967295;
result = (2147483647 - y)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2599: RHS constant
x = 2147483647;
result = (x - -4294967295)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test26() {
var x;
var y;
var result;
var check;
// Test 2600: both arguments variables
x = 2147483647;
y = -4294967296;
result = (x - y);
check = 6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2601: both arguments constants
result = (2147483647 - -4294967296)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2602: LHS constant
y = -4294967296;
result = (2147483647 - y)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2603: RHS constant
x = 2147483647;
result = (x - -4294967296)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2604: both arguments variables
x = 2147483648;
y = 0;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2605: both arguments constants
result = (2147483648 - 0)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2606: LHS constant
y = 0;
result = (2147483648 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2607: RHS constant
x = 2147483648;
result = (x - 0)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2608: both arguments variables
x = 2147483648;
y = 1;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2609: both arguments constants
result = (2147483648 - 1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2610: LHS constant
y = 1;
result = (2147483648 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2611: RHS constant
x = 2147483648;
result = (x - 1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2612: both arguments variables
x = 2147483648;
y = -1;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2613: both arguments constants
result = (2147483648 - -1)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2614: LHS constant
y = -1;
result = (2147483648 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2615: RHS constant
x = 2147483648;
result = (x - -1)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2616: both arguments variables
x = 2147483648;
y = 2;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2617: both arguments constants
result = (2147483648 - 2)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2618: LHS constant
y = 2;
result = (2147483648 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2619: RHS constant
x = 2147483648;
result = (x - 2)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2620: both arguments variables
x = 2147483648;
y = -2;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2621: both arguments constants
result = (2147483648 - -2)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2622: LHS constant
y = -2;
result = (2147483648 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2623: RHS constant
x = 2147483648;
result = (x - -2)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2624: both arguments variables
x = 2147483648;
y = 3;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2625: both arguments constants
result = (2147483648 - 3)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2626: LHS constant
y = 3;
result = (2147483648 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2627: RHS constant
x = 2147483648;
result = (x - 3)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2628: both arguments variables
x = 2147483648;
y = -3;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2629: both arguments constants
result = (2147483648 - -3)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2630: LHS constant
y = -3;
result = (2147483648 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2631: RHS constant
x = 2147483648;
result = (x - -3)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2632: both arguments variables
x = 2147483648;
y = 4;
result = (x - y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2633: both arguments constants
result = (2147483648 - 4)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2634: LHS constant
y = 4;
result = (2147483648 - y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2635: RHS constant
x = 2147483648;
result = (x - 4)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2636: both arguments variables
x = 2147483648;
y = -4;
result = (x - y);
check = 2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2637: both arguments constants
result = (2147483648 - -4)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2638: LHS constant
y = -4;
result = (2147483648 - y)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2639: RHS constant
x = 2147483648;
result = (x - -4)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2640: both arguments variables
x = 2147483648;
y = 8;
result = (x - y);
check = 2147483640;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2641: both arguments constants
result = (2147483648 - 8)
check = 2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2642: LHS constant
y = 8;
result = (2147483648 - y)
check = 2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2643: RHS constant
x = 2147483648;
result = (x - 8)
check = 2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2644: both arguments variables
x = 2147483648;
y = -8;
result = (x - y);
check = 2147483656;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2645: both arguments constants
result = (2147483648 - -8)
check = 2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2646: LHS constant
y = -8;
result = (2147483648 - y)
check = 2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2647: RHS constant
x = 2147483648;
result = (x - -8)
check = 2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2648: both arguments variables
x = 2147483648;
y = 1073741822;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2649: both arguments constants
result = (2147483648 - 1073741822)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2650: LHS constant
y = 1073741822;
result = (2147483648 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2651: RHS constant
x = 2147483648;
result = (x - 1073741822)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2652: both arguments variables
x = 2147483648;
y = 1073741823;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2653: both arguments constants
result = (2147483648 - 1073741823)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2654: LHS constant
y = 1073741823;
result = (2147483648 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2655: RHS constant
x = 2147483648;
result = (x - 1073741823)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2656: both arguments variables
x = 2147483648;
y = 1073741824;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2657: both arguments constants
result = (2147483648 - 1073741824)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2658: LHS constant
y = 1073741824;
result = (2147483648 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2659: RHS constant
x = 2147483648;
result = (x - 1073741824)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2660: both arguments variables
x = 2147483648;
y = 1073741825;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2661: both arguments constants
result = (2147483648 - 1073741825)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2662: LHS constant
y = 1073741825;
result = (2147483648 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2663: RHS constant
x = 2147483648;
result = (x - 1073741825)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2664: both arguments variables
x = 2147483648;
y = -1073741823;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2665: both arguments constants
result = (2147483648 - -1073741823)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2666: LHS constant
y = -1073741823;
result = (2147483648 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2667: RHS constant
x = 2147483648;
result = (x - -1073741823)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2668: both arguments variables
x = 2147483648;
y = (-0x3fffffff-1);
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2669: both arguments constants
result = (2147483648 - (-0x3fffffff-1))
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2670: LHS constant
y = (-0x3fffffff-1);
result = (2147483648 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2671: RHS constant
x = 2147483648;
result = (x - (-0x3fffffff-1))
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2672: both arguments variables
x = 2147483648;
y = -1073741825;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2673: both arguments constants
result = (2147483648 - -1073741825)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2674: LHS constant
y = -1073741825;
result = (2147483648 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2675: RHS constant
x = 2147483648;
result = (x - -1073741825)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2676: both arguments variables
x = 2147483648;
y = -1073741826;
result = (x - y);
check = 3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2677: both arguments constants
result = (2147483648 - -1073741826)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2678: LHS constant
y = -1073741826;
result = (2147483648 - y)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2679: RHS constant
x = 2147483648;
result = (x - -1073741826)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2680: both arguments variables
x = 2147483648;
y = 2147483646;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2681: both arguments constants
result = (2147483648 - 2147483646)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2682: LHS constant
y = 2147483646;
result = (2147483648 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2683: RHS constant
x = 2147483648;
result = (x - 2147483646)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2684: both arguments variables
x = 2147483648;
y = 2147483647;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2685: both arguments constants
result = (2147483648 - 2147483647)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2686: LHS constant
y = 2147483647;
result = (2147483648 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2687: RHS constant
x = 2147483648;
result = (x - 2147483647)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2688: both arguments variables
x = 2147483648;
y = 2147483648;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2689: both arguments constants
result = (2147483648 - 2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2690: LHS constant
y = 2147483648;
result = (2147483648 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2691: RHS constant
x = 2147483648;
result = (x - 2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2692: both arguments variables
x = 2147483648;
y = 2147483649;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2693: both arguments constants
result = (2147483648 - 2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2694: LHS constant
y = 2147483649;
result = (2147483648 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2695: RHS constant
x = 2147483648;
result = (x - 2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2696: both arguments variables
x = 2147483648;
y = -2147483647;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2697: both arguments constants
result = (2147483648 - -2147483647)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2698: LHS constant
y = -2147483647;
result = (2147483648 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2699: RHS constant
x = 2147483648;
result = (x - -2147483647)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test27() {
var x;
var y;
var result;
var check;
// Test 2700: both arguments variables
x = 2147483648;
y = -2147483648;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2701: both arguments constants
result = (2147483648 - -2147483648)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2702: LHS constant
y = -2147483648;
result = (2147483648 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2703: RHS constant
x = 2147483648;
result = (x - -2147483648)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2704: both arguments variables
x = 2147483648;
y = -2147483649;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2705: both arguments constants
result = (2147483648 - -2147483649)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2706: LHS constant
y = -2147483649;
result = (2147483648 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2707: RHS constant
x = 2147483648;
result = (x - -2147483649)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2708: both arguments variables
x = 2147483648;
y = -2147483650;
result = (x - y);
check = 4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2709: both arguments constants
result = (2147483648 - -2147483650)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2710: LHS constant
y = -2147483650;
result = (2147483648 - y)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2711: RHS constant
x = 2147483648;
result = (x - -2147483650)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2712: both arguments variables
x = 2147483648;
y = 4294967295;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2713: both arguments constants
result = (2147483648 - 4294967295)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2714: LHS constant
y = 4294967295;
result = (2147483648 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2715: RHS constant
x = 2147483648;
result = (x - 4294967295)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2716: both arguments variables
x = 2147483648;
y = 4294967296;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2717: both arguments constants
result = (2147483648 - 4294967296)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2718: LHS constant
y = 4294967296;
result = (2147483648 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2719: RHS constant
x = 2147483648;
result = (x - 4294967296)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2720: both arguments variables
x = 2147483648;
y = -4294967295;
result = (x - y);
check = 6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2721: both arguments constants
result = (2147483648 - -4294967295)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2722: LHS constant
y = -4294967295;
result = (2147483648 - y)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2723: RHS constant
x = 2147483648;
result = (x - -4294967295)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2724: both arguments variables
x = 2147483648;
y = -4294967296;
result = (x - y);
check = 6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2725: both arguments constants
result = (2147483648 - -4294967296)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2726: LHS constant
y = -4294967296;
result = (2147483648 - y)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2727: RHS constant
x = 2147483648;
result = (x - -4294967296)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2728: both arguments variables
x = 2147483649;
y = 0;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2729: both arguments constants
result = (2147483649 - 0)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2730: LHS constant
y = 0;
result = (2147483649 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2731: RHS constant
x = 2147483649;
result = (x - 0)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2732: both arguments variables
x = 2147483649;
y = 1;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2733: both arguments constants
result = (2147483649 - 1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2734: LHS constant
y = 1;
result = (2147483649 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2735: RHS constant
x = 2147483649;
result = (x - 1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2736: both arguments variables
x = 2147483649;
y = -1;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2737: both arguments constants
result = (2147483649 - -1)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2738: LHS constant
y = -1;
result = (2147483649 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2739: RHS constant
x = 2147483649;
result = (x - -1)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2740: both arguments variables
x = 2147483649;
y = 2;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2741: both arguments constants
result = (2147483649 - 2)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2742: LHS constant
y = 2;
result = (2147483649 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2743: RHS constant
x = 2147483649;
result = (x - 2)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2744: both arguments variables
x = 2147483649;
y = -2;
result = (x - y);
check = 2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2745: both arguments constants
result = (2147483649 - -2)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2746: LHS constant
y = -2;
result = (2147483649 - y)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2747: RHS constant
x = 2147483649;
result = (x - -2)
check = 2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2748: both arguments variables
x = 2147483649;
y = 3;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2749: both arguments constants
result = (2147483649 - 3)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2750: LHS constant
y = 3;
result = (2147483649 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2751: RHS constant
x = 2147483649;
result = (x - 3)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2752: both arguments variables
x = 2147483649;
y = -3;
result = (x - y);
check = 2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2753: both arguments constants
result = (2147483649 - -3)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2754: LHS constant
y = -3;
result = (2147483649 - y)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2755: RHS constant
x = 2147483649;
result = (x - -3)
check = 2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2756: both arguments variables
x = 2147483649;
y = 4;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2757: both arguments constants
result = (2147483649 - 4)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2758: LHS constant
y = 4;
result = (2147483649 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2759: RHS constant
x = 2147483649;
result = (x - 4)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2760: both arguments variables
x = 2147483649;
y = -4;
result = (x - y);
check = 2147483653;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2761: both arguments constants
result = (2147483649 - -4)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2762: LHS constant
y = -4;
result = (2147483649 - y)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2763: RHS constant
x = 2147483649;
result = (x - -4)
check = 2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2764: both arguments variables
x = 2147483649;
y = 8;
result = (x - y);
check = 2147483641;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2765: both arguments constants
result = (2147483649 - 8)
check = 2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2766: LHS constant
y = 8;
result = (2147483649 - y)
check = 2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2767: RHS constant
x = 2147483649;
result = (x - 8)
check = 2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2768: both arguments variables
x = 2147483649;
y = -8;
result = (x - y);
check = 2147483657;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2769: both arguments constants
result = (2147483649 - -8)
check = 2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2770: LHS constant
y = -8;
result = (2147483649 - y)
check = 2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2771: RHS constant
x = 2147483649;
result = (x - -8)
check = 2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2772: both arguments variables
x = 2147483649;
y = 1073741822;
result = (x - y);
check = 1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2773: both arguments constants
result = (2147483649 - 1073741822)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2774: LHS constant
y = 1073741822;
result = (2147483649 - y)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2775: RHS constant
x = 2147483649;
result = (x - 1073741822)
check = 1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2776: both arguments variables
x = 2147483649;
y = 1073741823;
result = (x - y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2777: both arguments constants
result = (2147483649 - 1073741823)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2778: LHS constant
y = 1073741823;
result = (2147483649 - y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2779: RHS constant
x = 2147483649;
result = (x - 1073741823)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2780: both arguments variables
x = 2147483649;
y = 1073741824;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2781: both arguments constants
result = (2147483649 - 1073741824)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2782: LHS constant
y = 1073741824;
result = (2147483649 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2783: RHS constant
x = 2147483649;
result = (x - 1073741824)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2784: both arguments variables
x = 2147483649;
y = 1073741825;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2785: both arguments constants
result = (2147483649 - 1073741825)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2786: LHS constant
y = 1073741825;
result = (2147483649 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2787: RHS constant
x = 2147483649;
result = (x - 1073741825)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2788: both arguments variables
x = 2147483649;
y = -1073741823;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2789: both arguments constants
result = (2147483649 - -1073741823)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2790: LHS constant
y = -1073741823;
result = (2147483649 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2791: RHS constant
x = 2147483649;
result = (x - -1073741823)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2792: both arguments variables
x = 2147483649;
y = (-0x3fffffff-1);
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2793: both arguments constants
result = (2147483649 - (-0x3fffffff-1))
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2794: LHS constant
y = (-0x3fffffff-1);
result = (2147483649 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2795: RHS constant
x = 2147483649;
result = (x - (-0x3fffffff-1))
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2796: both arguments variables
x = 2147483649;
y = -1073741825;
result = (x - y);
check = 3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2797: both arguments constants
result = (2147483649 - -1073741825)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2798: LHS constant
y = -1073741825;
result = (2147483649 - y)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2799: RHS constant
x = 2147483649;
result = (x - -1073741825)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test28() {
var x;
var y;
var result;
var check;
// Test 2800: both arguments variables
x = 2147483649;
y = -1073741826;
result = (x - y);
check = 3221225475;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2801: both arguments constants
result = (2147483649 - -1073741826)
check = 3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2802: LHS constant
y = -1073741826;
result = (2147483649 - y)
check = 3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2803: RHS constant
x = 2147483649;
result = (x - -1073741826)
check = 3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2804: both arguments variables
x = 2147483649;
y = 2147483646;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2805: both arguments constants
result = (2147483649 - 2147483646)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2806: LHS constant
y = 2147483646;
result = (2147483649 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2807: RHS constant
x = 2147483649;
result = (x - 2147483646)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2808: both arguments variables
x = 2147483649;
y = 2147483647;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2809: both arguments constants
result = (2147483649 - 2147483647)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2810: LHS constant
y = 2147483647;
result = (2147483649 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2811: RHS constant
x = 2147483649;
result = (x - 2147483647)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2812: both arguments variables
x = 2147483649;
y = 2147483648;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2813: both arguments constants
result = (2147483649 - 2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2814: LHS constant
y = 2147483648;
result = (2147483649 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2815: RHS constant
x = 2147483649;
result = (x - 2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2816: both arguments variables
x = 2147483649;
y = 2147483649;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2817: both arguments constants
result = (2147483649 - 2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2818: LHS constant
y = 2147483649;
result = (2147483649 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2819: RHS constant
x = 2147483649;
result = (x - 2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2820: both arguments variables
x = 2147483649;
y = -2147483647;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2821: both arguments constants
result = (2147483649 - -2147483647)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2822: LHS constant
y = -2147483647;
result = (2147483649 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2823: RHS constant
x = 2147483649;
result = (x - -2147483647)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2824: both arguments variables
x = 2147483649;
y = -2147483648;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2825: both arguments constants
result = (2147483649 - -2147483648)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2826: LHS constant
y = -2147483648;
result = (2147483649 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2827: RHS constant
x = 2147483649;
result = (x - -2147483648)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2828: both arguments variables
x = 2147483649;
y = -2147483649;
result = (x - y);
check = 4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2829: both arguments constants
result = (2147483649 - -2147483649)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2830: LHS constant
y = -2147483649;
result = (2147483649 - y)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2831: RHS constant
x = 2147483649;
result = (x - -2147483649)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2832: both arguments variables
x = 2147483649;
y = -2147483650;
result = (x - y);
check = 4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2833: both arguments constants
result = (2147483649 - -2147483650)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2834: LHS constant
y = -2147483650;
result = (2147483649 - y)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2835: RHS constant
x = 2147483649;
result = (x - -2147483650)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2836: both arguments variables
x = 2147483649;
y = 4294967295;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2837: both arguments constants
result = (2147483649 - 4294967295)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2838: LHS constant
y = 4294967295;
result = (2147483649 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2839: RHS constant
x = 2147483649;
result = (x - 4294967295)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2840: both arguments variables
x = 2147483649;
y = 4294967296;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2841: both arguments constants
result = (2147483649 - 4294967296)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2842: LHS constant
y = 4294967296;
result = (2147483649 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2843: RHS constant
x = 2147483649;
result = (x - 4294967296)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2844: both arguments variables
x = 2147483649;
y = -4294967295;
result = (x - y);
check = 6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2845: both arguments constants
result = (2147483649 - -4294967295)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2846: LHS constant
y = -4294967295;
result = (2147483649 - y)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2847: RHS constant
x = 2147483649;
result = (x - -4294967295)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2848: both arguments variables
x = 2147483649;
y = -4294967296;
result = (x - y);
check = 6442450945;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2849: both arguments constants
result = (2147483649 - -4294967296)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2850: LHS constant
y = -4294967296;
result = (2147483649 - y)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2851: RHS constant
x = 2147483649;
result = (x - -4294967296)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2852: both arguments variables
x = -2147483647;
y = 0;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2853: both arguments constants
result = (-2147483647 - 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2854: LHS constant
y = 0;
result = (-2147483647 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2855: RHS constant
x = -2147483647;
result = (x - 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2856: both arguments variables
x = -2147483647;
y = 1;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2857: both arguments constants
result = (-2147483647 - 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2858: LHS constant
y = 1;
result = (-2147483647 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2859: RHS constant
x = -2147483647;
result = (x - 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2860: both arguments variables
x = -2147483647;
y = -1;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2861: both arguments constants
result = (-2147483647 - -1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2862: LHS constant
y = -1;
result = (-2147483647 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2863: RHS constant
x = -2147483647;
result = (x - -1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2864: both arguments variables
x = -2147483647;
y = 2;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2865: both arguments constants
result = (-2147483647 - 2)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2866: LHS constant
y = 2;
result = (-2147483647 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2867: RHS constant
x = -2147483647;
result = (x - 2)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2868: both arguments variables
x = -2147483647;
y = -2;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2869: both arguments constants
result = (-2147483647 - -2)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2870: LHS constant
y = -2;
result = (-2147483647 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2871: RHS constant
x = -2147483647;
result = (x - -2)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2872: both arguments variables
x = -2147483647;
y = 3;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2873: both arguments constants
result = (-2147483647 - 3)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2874: LHS constant
y = 3;
result = (-2147483647 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2875: RHS constant
x = -2147483647;
result = (x - 3)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2876: both arguments variables
x = -2147483647;
y = -3;
result = (x - y);
check = -2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2877: both arguments constants
result = (-2147483647 - -3)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2878: LHS constant
y = -3;
result = (-2147483647 - y)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2879: RHS constant
x = -2147483647;
result = (x - -3)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2880: both arguments variables
x = -2147483647;
y = 4;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2881: both arguments constants
result = (-2147483647 - 4)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2882: LHS constant
y = 4;
result = (-2147483647 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2883: RHS constant
x = -2147483647;
result = (x - 4)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2884: both arguments variables
x = -2147483647;
y = -4;
result = (x - y);
check = -2147483643;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2885: both arguments constants
result = (-2147483647 - -4)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2886: LHS constant
y = -4;
result = (-2147483647 - y)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2887: RHS constant
x = -2147483647;
result = (x - -4)
check = -2147483643
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2888: both arguments variables
x = -2147483647;
y = 8;
result = (x - y);
check = -2147483655;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2889: both arguments constants
result = (-2147483647 - 8)
check = -2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2890: LHS constant
y = 8;
result = (-2147483647 - y)
check = -2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2891: RHS constant
x = -2147483647;
result = (x - 8)
check = -2147483655
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2892: both arguments variables
x = -2147483647;
y = -8;
result = (x - y);
check = -2147483639;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2893: both arguments constants
result = (-2147483647 - -8)
check = -2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2894: LHS constant
y = -8;
result = (-2147483647 - y)
check = -2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2895: RHS constant
x = -2147483647;
result = (x - -8)
check = -2147483639
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2896: both arguments variables
x = -2147483647;
y = 1073741822;
result = (x - y);
check = -3221225469;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2897: both arguments constants
result = (-2147483647 - 1073741822)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2898: LHS constant
y = 1073741822;
result = (-2147483647 - y)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2899: RHS constant
x = -2147483647;
result = (x - 1073741822)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test29() {
var x;
var y;
var result;
var check;
// Test 2900: both arguments variables
x = -2147483647;
y = 1073741823;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2901: both arguments constants
result = (-2147483647 - 1073741823)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2902: LHS constant
y = 1073741823;
result = (-2147483647 - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2903: RHS constant
x = -2147483647;
result = (x - 1073741823)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2904: both arguments variables
x = -2147483647;
y = 1073741824;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2905: both arguments constants
result = (-2147483647 - 1073741824)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2906: LHS constant
y = 1073741824;
result = (-2147483647 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2907: RHS constant
x = -2147483647;
result = (x - 1073741824)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2908: both arguments variables
x = -2147483647;
y = 1073741825;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2909: both arguments constants
result = (-2147483647 - 1073741825)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2910: LHS constant
y = 1073741825;
result = (-2147483647 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2911: RHS constant
x = -2147483647;
result = (x - 1073741825)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2912: both arguments variables
x = -2147483647;
y = -1073741823;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2913: both arguments constants
result = (-2147483647 - -1073741823)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2914: LHS constant
y = -1073741823;
result = (-2147483647 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2915: RHS constant
x = -2147483647;
result = (x - -1073741823)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2916: both arguments variables
x = -2147483647;
y = (-0x3fffffff-1);
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2917: both arguments constants
result = (-2147483647 - (-0x3fffffff-1))
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2918: LHS constant
y = (-0x3fffffff-1);
result = (-2147483647 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2919: RHS constant
x = -2147483647;
result = (x - (-0x3fffffff-1))
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2920: both arguments variables
x = -2147483647;
y = -1073741825;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2921: both arguments constants
result = (-2147483647 - -1073741825)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2922: LHS constant
y = -1073741825;
result = (-2147483647 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2923: RHS constant
x = -2147483647;
result = (x - -1073741825)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2924: both arguments variables
x = -2147483647;
y = -1073741826;
result = (x - y);
check = -1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2925: both arguments constants
result = (-2147483647 - -1073741826)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2926: LHS constant
y = -1073741826;
result = (-2147483647 - y)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2927: RHS constant
x = -2147483647;
result = (x - -1073741826)
check = -1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2928: both arguments variables
x = -2147483647;
y = 2147483646;
result = (x - y);
check = -4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2929: both arguments constants
result = (-2147483647 - 2147483646)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2930: LHS constant
y = 2147483646;
result = (-2147483647 - y)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2931: RHS constant
x = -2147483647;
result = (x - 2147483646)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2932: both arguments variables
x = -2147483647;
y = 2147483647;
result = (x - y);
check = -4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2933: both arguments constants
result = (-2147483647 - 2147483647)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2934: LHS constant
y = 2147483647;
result = (-2147483647 - y)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2935: RHS constant
x = -2147483647;
result = (x - 2147483647)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2936: both arguments variables
x = -2147483647;
y = 2147483648;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2937: both arguments constants
result = (-2147483647 - 2147483648)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2938: LHS constant
y = 2147483648;
result = (-2147483647 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2939: RHS constant
x = -2147483647;
result = (x - 2147483648)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2940: both arguments variables
x = -2147483647;
y = 2147483649;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2941: both arguments constants
result = (-2147483647 - 2147483649)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2942: LHS constant
y = 2147483649;
result = (-2147483647 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2943: RHS constant
x = -2147483647;
result = (x - 2147483649)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2944: both arguments variables
x = -2147483647;
y = -2147483647;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2945: both arguments constants
result = (-2147483647 - -2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2946: LHS constant
y = -2147483647;
result = (-2147483647 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2947: RHS constant
x = -2147483647;
result = (x - -2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2948: both arguments variables
x = -2147483647;
y = -2147483648;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2949: both arguments constants
result = (-2147483647 - -2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2950: LHS constant
y = -2147483648;
result = (-2147483647 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2951: RHS constant
x = -2147483647;
result = (x - -2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2952: both arguments variables
x = -2147483647;
y = -2147483649;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2953: both arguments constants
result = (-2147483647 - -2147483649)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2954: LHS constant
y = -2147483649;
result = (-2147483647 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2955: RHS constant
x = -2147483647;
result = (x - -2147483649)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2956: both arguments variables
x = -2147483647;
y = -2147483650;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2957: both arguments constants
result = (-2147483647 - -2147483650)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2958: LHS constant
y = -2147483650;
result = (-2147483647 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2959: RHS constant
x = -2147483647;
result = (x - -2147483650)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2960: both arguments variables
x = -2147483647;
y = 4294967295;
result = (x - y);
check = -6442450942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2961: both arguments constants
result = (-2147483647 - 4294967295)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2962: LHS constant
y = 4294967295;
result = (-2147483647 - y)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2963: RHS constant
x = -2147483647;
result = (x - 4294967295)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2964: both arguments variables
x = -2147483647;
y = 4294967296;
result = (x - y);
check = -6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2965: both arguments constants
result = (-2147483647 - 4294967296)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2966: LHS constant
y = 4294967296;
result = (-2147483647 - y)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2967: RHS constant
x = -2147483647;
result = (x - 4294967296)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2968: both arguments variables
x = -2147483647;
y = -4294967295;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2969: both arguments constants
result = (-2147483647 - -4294967295)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2970: LHS constant
y = -4294967295;
result = (-2147483647 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2971: RHS constant
x = -2147483647;
result = (x - -4294967295)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2972: both arguments variables
x = -2147483647;
y = -4294967296;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2973: both arguments constants
result = (-2147483647 - -4294967296)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2974: LHS constant
y = -4294967296;
result = (-2147483647 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2975: RHS constant
x = -2147483647;
result = (x - -4294967296)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2976: both arguments variables
x = -2147483648;
y = 0;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2977: both arguments constants
result = (-2147483648 - 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2978: LHS constant
y = 0;
result = (-2147483648 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2979: RHS constant
x = -2147483648;
result = (x - 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2980: both arguments variables
x = -2147483648;
y = 1;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2981: both arguments constants
result = (-2147483648 - 1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2982: LHS constant
y = 1;
result = (-2147483648 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2983: RHS constant
x = -2147483648;
result = (x - 1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2984: both arguments variables
x = -2147483648;
y = -1;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2985: both arguments constants
result = (-2147483648 - -1)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2986: LHS constant
y = -1;
result = (-2147483648 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2987: RHS constant
x = -2147483648;
result = (x - -1)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2988: both arguments variables
x = -2147483648;
y = 2;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2989: both arguments constants
result = (-2147483648 - 2)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2990: LHS constant
y = 2;
result = (-2147483648 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2991: RHS constant
x = -2147483648;
result = (x - 2)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2992: both arguments variables
x = -2147483648;
y = -2;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2993: both arguments constants
result = (-2147483648 - -2)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2994: LHS constant
y = -2;
result = (-2147483648 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2995: RHS constant
x = -2147483648;
result = (x - -2)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2996: both arguments variables
x = -2147483648;
y = 3;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 2997: both arguments constants
result = (-2147483648 - 3)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2998: LHS constant
y = 3;
result = (-2147483648 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2999: RHS constant
x = -2147483648;
result = (x - 3)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test30() {
var x;
var y;
var result;
var check;
// Test 3000: both arguments variables
x = -2147483648;
y = -3;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3001: both arguments constants
result = (-2147483648 - -3)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3002: LHS constant
y = -3;
result = (-2147483648 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3003: RHS constant
x = -2147483648;
result = (x - -3)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3004: both arguments variables
x = -2147483648;
y = 4;
result = (x - y);
check = -2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3005: both arguments constants
result = (-2147483648 - 4)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3006: LHS constant
y = 4;
result = (-2147483648 - y)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3007: RHS constant
x = -2147483648;
result = (x - 4)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3008: both arguments variables
x = -2147483648;
y = -4;
result = (x - y);
check = -2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3009: both arguments constants
result = (-2147483648 - -4)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3010: LHS constant
y = -4;
result = (-2147483648 - y)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3011: RHS constant
x = -2147483648;
result = (x - -4)
check = -2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3012: both arguments variables
x = -2147483648;
y = 8;
result = (x - y);
check = -2147483656;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3013: both arguments constants
result = (-2147483648 - 8)
check = -2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3014: LHS constant
y = 8;
result = (-2147483648 - y)
check = -2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3015: RHS constant
x = -2147483648;
result = (x - 8)
check = -2147483656
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3016: both arguments variables
x = -2147483648;
y = -8;
result = (x - y);
check = -2147483640;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3017: both arguments constants
result = (-2147483648 - -8)
check = -2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3018: LHS constant
y = -8;
result = (-2147483648 - y)
check = -2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3019: RHS constant
x = -2147483648;
result = (x - -8)
check = -2147483640
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3020: both arguments variables
x = -2147483648;
y = 1073741822;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3021: both arguments constants
result = (-2147483648 - 1073741822)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3022: LHS constant
y = 1073741822;
result = (-2147483648 - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3023: RHS constant
x = -2147483648;
result = (x - 1073741822)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3024: both arguments variables
x = -2147483648;
y = 1073741823;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3025: both arguments constants
result = (-2147483648 - 1073741823)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3026: LHS constant
y = 1073741823;
result = (-2147483648 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3027: RHS constant
x = -2147483648;
result = (x - 1073741823)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3028: both arguments variables
x = -2147483648;
y = 1073741824;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3029: both arguments constants
result = (-2147483648 - 1073741824)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3030: LHS constant
y = 1073741824;
result = (-2147483648 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3031: RHS constant
x = -2147483648;
result = (x - 1073741824)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3032: both arguments variables
x = -2147483648;
y = 1073741825;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3033: both arguments constants
result = (-2147483648 - 1073741825)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3034: LHS constant
y = 1073741825;
result = (-2147483648 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3035: RHS constant
x = -2147483648;
result = (x - 1073741825)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3036: both arguments variables
x = -2147483648;
y = -1073741823;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3037: both arguments constants
result = (-2147483648 - -1073741823)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3038: LHS constant
y = -1073741823;
result = (-2147483648 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3039: RHS constant
x = -2147483648;
result = (x - -1073741823)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3040: both arguments variables
x = -2147483648;
y = (-0x3fffffff-1);
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3041: both arguments constants
result = (-2147483648 - (-0x3fffffff-1))
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3042: LHS constant
y = (-0x3fffffff-1);
result = (-2147483648 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3043: RHS constant
x = -2147483648;
result = (x - (-0x3fffffff-1))
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3044: both arguments variables
x = -2147483648;
y = -1073741825;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3045: both arguments constants
result = (-2147483648 - -1073741825)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3046: LHS constant
y = -1073741825;
result = (-2147483648 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3047: RHS constant
x = -2147483648;
result = (x - -1073741825)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3048: both arguments variables
x = -2147483648;
y = -1073741826;
result = (x - y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3049: both arguments constants
result = (-2147483648 - -1073741826)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3050: LHS constant
y = -1073741826;
result = (-2147483648 - y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3051: RHS constant
x = -2147483648;
result = (x - -1073741826)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3052: both arguments variables
x = -2147483648;
y = 2147483646;
result = (x - y);
check = -4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3053: both arguments constants
result = (-2147483648 - 2147483646)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3054: LHS constant
y = 2147483646;
result = (-2147483648 - y)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3055: RHS constant
x = -2147483648;
result = (x - 2147483646)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3056: both arguments variables
x = -2147483648;
y = 2147483647;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3057: both arguments constants
result = (-2147483648 - 2147483647)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3058: LHS constant
y = 2147483647;
result = (-2147483648 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3059: RHS constant
x = -2147483648;
result = (x - 2147483647)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3060: both arguments variables
x = -2147483648;
y = 2147483648;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3061: both arguments constants
result = (-2147483648 - 2147483648)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3062: LHS constant
y = 2147483648;
result = (-2147483648 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3063: RHS constant
x = -2147483648;
result = (x - 2147483648)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3064: both arguments variables
x = -2147483648;
y = 2147483649;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3065: both arguments constants
result = (-2147483648 - 2147483649)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3066: LHS constant
y = 2147483649;
result = (-2147483648 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3067: RHS constant
x = -2147483648;
result = (x - 2147483649)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3068: both arguments variables
x = -2147483648;
y = -2147483647;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3069: both arguments constants
result = (-2147483648 - -2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3070: LHS constant
y = -2147483647;
result = (-2147483648 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3071: RHS constant
x = -2147483648;
result = (x - -2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3072: both arguments variables
x = -2147483648;
y = -2147483648;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3073: both arguments constants
result = (-2147483648 - -2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3074: LHS constant
y = -2147483648;
result = (-2147483648 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3075: RHS constant
x = -2147483648;
result = (x - -2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3076: both arguments variables
x = -2147483648;
y = -2147483649;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3077: both arguments constants
result = (-2147483648 - -2147483649)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3078: LHS constant
y = -2147483649;
result = (-2147483648 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3079: RHS constant
x = -2147483648;
result = (x - -2147483649)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3080: both arguments variables
x = -2147483648;
y = -2147483650;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3081: both arguments constants
result = (-2147483648 - -2147483650)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3082: LHS constant
y = -2147483650;
result = (-2147483648 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3083: RHS constant
x = -2147483648;
result = (x - -2147483650)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3084: both arguments variables
x = -2147483648;
y = 4294967295;
result = (x - y);
check = -6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3085: both arguments constants
result = (-2147483648 - 4294967295)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3086: LHS constant
y = 4294967295;
result = (-2147483648 - y)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3087: RHS constant
x = -2147483648;
result = (x - 4294967295)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3088: both arguments variables
x = -2147483648;
y = 4294967296;
result = (x - y);
check = -6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3089: both arguments constants
result = (-2147483648 - 4294967296)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3090: LHS constant
y = 4294967296;
result = (-2147483648 - y)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3091: RHS constant
x = -2147483648;
result = (x - 4294967296)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3092: both arguments variables
x = -2147483648;
y = -4294967295;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3093: both arguments constants
result = (-2147483648 - -4294967295)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3094: LHS constant
y = -4294967295;
result = (-2147483648 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3095: RHS constant
x = -2147483648;
result = (x - -4294967295)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3096: both arguments variables
x = -2147483648;
y = -4294967296;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3097: both arguments constants
result = (-2147483648 - -4294967296)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3098: LHS constant
y = -4294967296;
result = (-2147483648 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3099: RHS constant
x = -2147483648;
result = (x - -4294967296)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test31() {
var x;
var y;
var result;
var check;
// Test 3100: both arguments variables
x = -2147483649;
y = 0;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3101: both arguments constants
result = (-2147483649 - 0)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3102: LHS constant
y = 0;
result = (-2147483649 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3103: RHS constant
x = -2147483649;
result = (x - 0)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3104: both arguments variables
x = -2147483649;
y = 1;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3105: both arguments constants
result = (-2147483649 - 1)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3106: LHS constant
y = 1;
result = (-2147483649 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3107: RHS constant
x = -2147483649;
result = (x - 1)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3108: both arguments variables
x = -2147483649;
y = -1;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3109: both arguments constants
result = (-2147483649 - -1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3110: LHS constant
y = -1;
result = (-2147483649 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3111: RHS constant
x = -2147483649;
result = (x - -1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3112: both arguments variables
x = -2147483649;
y = 2;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3113: both arguments constants
result = (-2147483649 - 2)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3114: LHS constant
y = 2;
result = (-2147483649 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3115: RHS constant
x = -2147483649;
result = (x - 2)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3116: both arguments variables
x = -2147483649;
y = -2;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3117: both arguments constants
result = (-2147483649 - -2)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3118: LHS constant
y = -2;
result = (-2147483649 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3119: RHS constant
x = -2147483649;
result = (x - -2)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3120: both arguments variables
x = -2147483649;
y = 3;
result = (x - y);
check = -2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3121: both arguments constants
result = (-2147483649 - 3)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3122: LHS constant
y = 3;
result = (-2147483649 - y)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3123: RHS constant
x = -2147483649;
result = (x - 3)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3124: both arguments variables
x = -2147483649;
y = -3;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3125: both arguments constants
result = (-2147483649 - -3)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3126: LHS constant
y = -3;
result = (-2147483649 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3127: RHS constant
x = -2147483649;
result = (x - -3)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3128: both arguments variables
x = -2147483649;
y = 4;
result = (x - y);
check = -2147483653;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3129: both arguments constants
result = (-2147483649 - 4)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3130: LHS constant
y = 4;
result = (-2147483649 - y)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3131: RHS constant
x = -2147483649;
result = (x - 4)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3132: both arguments variables
x = -2147483649;
y = -4;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3133: both arguments constants
result = (-2147483649 - -4)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3134: LHS constant
y = -4;
result = (-2147483649 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3135: RHS constant
x = -2147483649;
result = (x - -4)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3136: both arguments variables
x = -2147483649;
y = 8;
result = (x - y);
check = -2147483657;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3137: both arguments constants
result = (-2147483649 - 8)
check = -2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3138: LHS constant
y = 8;
result = (-2147483649 - y)
check = -2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3139: RHS constant
x = -2147483649;
result = (x - 8)
check = -2147483657
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3140: both arguments variables
x = -2147483649;
y = -8;
result = (x - y);
check = -2147483641;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3141: both arguments constants
result = (-2147483649 - -8)
check = -2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3142: LHS constant
y = -8;
result = (-2147483649 - y)
check = -2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3143: RHS constant
x = -2147483649;
result = (x - -8)
check = -2147483641
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3144: both arguments variables
x = -2147483649;
y = 1073741822;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3145: both arguments constants
result = (-2147483649 - 1073741822)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3146: LHS constant
y = 1073741822;
result = (-2147483649 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3147: RHS constant
x = -2147483649;
result = (x - 1073741822)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3148: both arguments variables
x = -2147483649;
y = 1073741823;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3149: both arguments constants
result = (-2147483649 - 1073741823)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3150: LHS constant
y = 1073741823;
result = (-2147483649 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3151: RHS constant
x = -2147483649;
result = (x - 1073741823)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3152: both arguments variables
x = -2147483649;
y = 1073741824;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3153: both arguments constants
result = (-2147483649 - 1073741824)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3154: LHS constant
y = 1073741824;
result = (-2147483649 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3155: RHS constant
x = -2147483649;
result = (x - 1073741824)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3156: both arguments variables
x = -2147483649;
y = 1073741825;
result = (x - y);
check = -3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3157: both arguments constants
result = (-2147483649 - 1073741825)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3158: LHS constant
y = 1073741825;
result = (-2147483649 - y)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3159: RHS constant
x = -2147483649;
result = (x - 1073741825)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3160: both arguments variables
x = -2147483649;
y = -1073741823;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3161: both arguments constants
result = (-2147483649 - -1073741823)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3162: LHS constant
y = -1073741823;
result = (-2147483649 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3163: RHS constant
x = -2147483649;
result = (x - -1073741823)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3164: both arguments variables
x = -2147483649;
y = (-0x3fffffff-1);
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3165: both arguments constants
result = (-2147483649 - (-0x3fffffff-1))
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3166: LHS constant
y = (-0x3fffffff-1);
result = (-2147483649 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3167: RHS constant
x = -2147483649;
result = (x - (-0x3fffffff-1))
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3168: both arguments variables
x = -2147483649;
y = -1073741825;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3169: both arguments constants
result = (-2147483649 - -1073741825)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3170: LHS constant
y = -1073741825;
result = (-2147483649 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3171: RHS constant
x = -2147483649;
result = (x - -1073741825)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3172: both arguments variables
x = -2147483649;
y = -1073741826;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3173: both arguments constants
result = (-2147483649 - -1073741826)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3174: LHS constant
y = -1073741826;
result = (-2147483649 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3175: RHS constant
x = -2147483649;
result = (x - -1073741826)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3176: both arguments variables
x = -2147483649;
y = 2147483646;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3177: both arguments constants
result = (-2147483649 - 2147483646)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3178: LHS constant
y = 2147483646;
result = (-2147483649 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3179: RHS constant
x = -2147483649;
result = (x - 2147483646)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3180: both arguments variables
x = -2147483649;
y = 2147483647;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3181: both arguments constants
result = (-2147483649 - 2147483647)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3182: LHS constant
y = 2147483647;
result = (-2147483649 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3183: RHS constant
x = -2147483649;
result = (x - 2147483647)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3184: both arguments variables
x = -2147483649;
y = 2147483648;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3185: both arguments constants
result = (-2147483649 - 2147483648)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3186: LHS constant
y = 2147483648;
result = (-2147483649 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3187: RHS constant
x = -2147483649;
result = (x - 2147483648)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3188: both arguments variables
x = -2147483649;
y = 2147483649;
result = (x - y);
check = -4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3189: both arguments constants
result = (-2147483649 - 2147483649)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3190: LHS constant
y = 2147483649;
result = (-2147483649 - y)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3191: RHS constant
x = -2147483649;
result = (x - 2147483649)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3192: both arguments variables
x = -2147483649;
y = -2147483647;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3193: both arguments constants
result = (-2147483649 - -2147483647)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3194: LHS constant
y = -2147483647;
result = (-2147483649 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3195: RHS constant
x = -2147483649;
result = (x - -2147483647)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3196: both arguments variables
x = -2147483649;
y = -2147483648;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3197: both arguments constants
result = (-2147483649 - -2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3198: LHS constant
y = -2147483648;
result = (-2147483649 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3199: RHS constant
x = -2147483649;
result = (x - -2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test32() {
var x;
var y;
var result;
var check;
// Test 3200: both arguments variables
x = -2147483649;
y = -2147483649;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3201: both arguments constants
result = (-2147483649 - -2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3202: LHS constant
y = -2147483649;
result = (-2147483649 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3203: RHS constant
x = -2147483649;
result = (x - -2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3204: both arguments variables
x = -2147483649;
y = -2147483650;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3205: both arguments constants
result = (-2147483649 - -2147483650)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3206: LHS constant
y = -2147483650;
result = (-2147483649 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3207: RHS constant
x = -2147483649;
result = (x - -2147483650)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3208: both arguments variables
x = -2147483649;
y = 4294967295;
result = (x - y);
check = -6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3209: both arguments constants
result = (-2147483649 - 4294967295)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3210: LHS constant
y = 4294967295;
result = (-2147483649 - y)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3211: RHS constant
x = -2147483649;
result = (x - 4294967295)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3212: both arguments variables
x = -2147483649;
y = 4294967296;
result = (x - y);
check = -6442450945;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3213: both arguments constants
result = (-2147483649 - 4294967296)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3214: LHS constant
y = 4294967296;
result = (-2147483649 - y)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3215: RHS constant
x = -2147483649;
result = (x - 4294967296)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3216: both arguments variables
x = -2147483649;
y = -4294967295;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3217: both arguments constants
result = (-2147483649 - -4294967295)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3218: LHS constant
y = -4294967295;
result = (-2147483649 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3219: RHS constant
x = -2147483649;
result = (x - -4294967295)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3220: both arguments variables
x = -2147483649;
y = -4294967296;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3221: both arguments constants
result = (-2147483649 - -4294967296)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3222: LHS constant
y = -4294967296;
result = (-2147483649 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3223: RHS constant
x = -2147483649;
result = (x - -4294967296)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3224: both arguments variables
x = -2147483650;
y = 0;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3225: both arguments constants
result = (-2147483650 - 0)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3226: LHS constant
y = 0;
result = (-2147483650 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3227: RHS constant
x = -2147483650;
result = (x - 0)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3228: both arguments variables
x = -2147483650;
y = 1;
result = (x - y);
check = -2147483651;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3229: both arguments constants
result = (-2147483650 - 1)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3230: LHS constant
y = 1;
result = (-2147483650 - y)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3231: RHS constant
x = -2147483650;
result = (x - 1)
check = -2147483651
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3232: both arguments variables
x = -2147483650;
y = -1;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3233: both arguments constants
result = (-2147483650 - -1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3234: LHS constant
y = -1;
result = (-2147483650 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3235: RHS constant
x = -2147483650;
result = (x - -1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3236: both arguments variables
x = -2147483650;
y = 2;
result = (x - y);
check = -2147483652;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3237: both arguments constants
result = (-2147483650 - 2)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3238: LHS constant
y = 2;
result = (-2147483650 - y)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3239: RHS constant
x = -2147483650;
result = (x - 2)
check = -2147483652
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3240: both arguments variables
x = -2147483650;
y = -2;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3241: both arguments constants
result = (-2147483650 - -2)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3242: LHS constant
y = -2;
result = (-2147483650 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3243: RHS constant
x = -2147483650;
result = (x - -2)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3244: both arguments variables
x = -2147483650;
y = 3;
result = (x - y);
check = -2147483653;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3245: both arguments constants
result = (-2147483650 - 3)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3246: LHS constant
y = 3;
result = (-2147483650 - y)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3247: RHS constant
x = -2147483650;
result = (x - 3)
check = -2147483653
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3248: both arguments variables
x = -2147483650;
y = -3;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3249: both arguments constants
result = (-2147483650 - -3)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3250: LHS constant
y = -3;
result = (-2147483650 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3251: RHS constant
x = -2147483650;
result = (x - -3)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3252: both arguments variables
x = -2147483650;
y = 4;
result = (x - y);
check = -2147483654;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3253: both arguments constants
result = (-2147483650 - 4)
check = -2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3254: LHS constant
y = 4;
result = (-2147483650 - y)
check = -2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3255: RHS constant
x = -2147483650;
result = (x - 4)
check = -2147483654
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3256: both arguments variables
x = -2147483650;
y = -4;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3257: both arguments constants
result = (-2147483650 - -4)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3258: LHS constant
y = -4;
result = (-2147483650 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3259: RHS constant
x = -2147483650;
result = (x - -4)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3260: both arguments variables
x = -2147483650;
y = 8;
result = (x - y);
check = -2147483658;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3261: both arguments constants
result = (-2147483650 - 8)
check = -2147483658
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3262: LHS constant
y = 8;
result = (-2147483650 - y)
check = -2147483658
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3263: RHS constant
x = -2147483650;
result = (x - 8)
check = -2147483658
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3264: both arguments variables
x = -2147483650;
y = -8;
result = (x - y);
check = -2147483642;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3265: both arguments constants
result = (-2147483650 - -8)
check = -2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3266: LHS constant
y = -8;
result = (-2147483650 - y)
check = -2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3267: RHS constant
x = -2147483650;
result = (x - -8)
check = -2147483642
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3268: both arguments variables
x = -2147483650;
y = 1073741822;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3269: both arguments constants
result = (-2147483650 - 1073741822)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3270: LHS constant
y = 1073741822;
result = (-2147483650 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3271: RHS constant
x = -2147483650;
result = (x - 1073741822)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3272: both arguments variables
x = -2147483650;
y = 1073741823;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3273: both arguments constants
result = (-2147483650 - 1073741823)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3274: LHS constant
y = 1073741823;
result = (-2147483650 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3275: RHS constant
x = -2147483650;
result = (x - 1073741823)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3276: both arguments variables
x = -2147483650;
y = 1073741824;
result = (x - y);
check = -3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3277: both arguments constants
result = (-2147483650 - 1073741824)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3278: LHS constant
y = 1073741824;
result = (-2147483650 - y)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3279: RHS constant
x = -2147483650;
result = (x - 1073741824)
check = -3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3280: both arguments variables
x = -2147483650;
y = 1073741825;
result = (x - y);
check = -3221225475;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3281: both arguments constants
result = (-2147483650 - 1073741825)
check = -3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3282: LHS constant
y = 1073741825;
result = (-2147483650 - y)
check = -3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3283: RHS constant
x = -2147483650;
result = (x - 1073741825)
check = -3221225475
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3284: both arguments variables
x = -2147483650;
y = -1073741823;
result = (x - y);
check = -1073741827;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3285: both arguments constants
result = (-2147483650 - -1073741823)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3286: LHS constant
y = -1073741823;
result = (-2147483650 - y)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3287: RHS constant
x = -2147483650;
result = (x - -1073741823)
check = -1073741827
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3288: both arguments variables
x = -2147483650;
y = (-0x3fffffff-1);
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3289: both arguments constants
result = (-2147483650 - (-0x3fffffff-1))
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3290: LHS constant
y = (-0x3fffffff-1);
result = (-2147483650 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3291: RHS constant
x = -2147483650;
result = (x - (-0x3fffffff-1))
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3292: both arguments variables
x = -2147483650;
y = -1073741825;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3293: both arguments constants
result = (-2147483650 - -1073741825)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3294: LHS constant
y = -1073741825;
result = (-2147483650 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3295: RHS constant
x = -2147483650;
result = (x - -1073741825)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3296: both arguments variables
x = -2147483650;
y = -1073741826;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3297: both arguments constants
result = (-2147483650 - -1073741826)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3298: LHS constant
y = -1073741826;
result = (-2147483650 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3299: RHS constant
x = -2147483650;
result = (x - -1073741826)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test33() {
var x;
var y;
var result;
var check;
// Test 3300: both arguments variables
x = -2147483650;
y = 2147483646;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3301: both arguments constants
result = (-2147483650 - 2147483646)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3302: LHS constant
y = 2147483646;
result = (-2147483650 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3303: RHS constant
x = -2147483650;
result = (x - 2147483646)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3304: both arguments variables
x = -2147483650;
y = 2147483647;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3305: both arguments constants
result = (-2147483650 - 2147483647)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3306: LHS constant
y = 2147483647;
result = (-2147483650 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3307: RHS constant
x = -2147483650;
result = (x - 2147483647)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3308: both arguments variables
x = -2147483650;
y = 2147483648;
result = (x - y);
check = -4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3309: both arguments constants
result = (-2147483650 - 2147483648)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3310: LHS constant
y = 2147483648;
result = (-2147483650 - y)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3311: RHS constant
x = -2147483650;
result = (x - 2147483648)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3312: both arguments variables
x = -2147483650;
y = 2147483649;
result = (x - y);
check = -4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3313: both arguments constants
result = (-2147483650 - 2147483649)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3314: LHS constant
y = 2147483649;
result = (-2147483650 - y)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3315: RHS constant
x = -2147483650;
result = (x - 2147483649)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3316: both arguments variables
x = -2147483650;
y = -2147483647;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3317: both arguments constants
result = (-2147483650 - -2147483647)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3318: LHS constant
y = -2147483647;
result = (-2147483650 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3319: RHS constant
x = -2147483650;
result = (x - -2147483647)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3320: both arguments variables
x = -2147483650;
y = -2147483648;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3321: both arguments constants
result = (-2147483650 - -2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3322: LHS constant
y = -2147483648;
result = (-2147483650 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3323: RHS constant
x = -2147483650;
result = (x - -2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3324: both arguments variables
x = -2147483650;
y = -2147483649;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3325: both arguments constants
result = (-2147483650 - -2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3326: LHS constant
y = -2147483649;
result = (-2147483650 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3327: RHS constant
x = -2147483650;
result = (x - -2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3328: both arguments variables
x = -2147483650;
y = -2147483650;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3329: both arguments constants
result = (-2147483650 - -2147483650)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3330: LHS constant
y = -2147483650;
result = (-2147483650 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3331: RHS constant
x = -2147483650;
result = (x - -2147483650)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3332: both arguments variables
x = -2147483650;
y = 4294967295;
result = (x - y);
check = -6442450945;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3333: both arguments constants
result = (-2147483650 - 4294967295)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3334: LHS constant
y = 4294967295;
result = (-2147483650 - y)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3335: RHS constant
x = -2147483650;
result = (x - 4294967295)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3336: both arguments variables
x = -2147483650;
y = 4294967296;
result = (x - y);
check = -6442450946;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3337: both arguments constants
result = (-2147483650 - 4294967296)
check = -6442450946
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3338: LHS constant
y = 4294967296;
result = (-2147483650 - y)
check = -6442450946
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3339: RHS constant
x = -2147483650;
result = (x - 4294967296)
check = -6442450946
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3340: both arguments variables
x = -2147483650;
y = -4294967295;
result = (x - y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3341: both arguments constants
result = (-2147483650 - -4294967295)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3342: LHS constant
y = -4294967295;
result = (-2147483650 - y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3343: RHS constant
x = -2147483650;
result = (x - -4294967295)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3344: both arguments variables
x = -2147483650;
y = -4294967296;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3345: both arguments constants
result = (-2147483650 - -4294967296)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3346: LHS constant
y = -4294967296;
result = (-2147483650 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3347: RHS constant
x = -2147483650;
result = (x - -4294967296)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3348: both arguments variables
x = 4294967295;
y = 0;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3349: both arguments constants
result = (4294967295 - 0)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3350: LHS constant
y = 0;
result = (4294967295 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3351: RHS constant
x = 4294967295;
result = (x - 0)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3352: both arguments variables
x = 4294967295;
y = 1;
result = (x - y);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3353: both arguments constants
result = (4294967295 - 1)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3354: LHS constant
y = 1;
result = (4294967295 - y)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3355: RHS constant
x = 4294967295;
result = (x - 1)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3356: both arguments variables
x = 4294967295;
y = -1;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3357: both arguments constants
result = (4294967295 - -1)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3358: LHS constant
y = -1;
result = (4294967295 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3359: RHS constant
x = 4294967295;
result = (x - -1)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3360: both arguments variables
x = 4294967295;
y = 2;
result = (x - y);
check = 4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3361: both arguments constants
result = (4294967295 - 2)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3362: LHS constant
y = 2;
result = (4294967295 - y)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3363: RHS constant
x = 4294967295;
result = (x - 2)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3364: both arguments variables
x = 4294967295;
y = -2;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3365: both arguments constants
result = (4294967295 - -2)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3366: LHS constant
y = -2;
result = (4294967295 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3367: RHS constant
x = 4294967295;
result = (x - -2)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3368: both arguments variables
x = 4294967295;
y = 3;
result = (x - y);
check = 4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3369: both arguments constants
result = (4294967295 - 3)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3370: LHS constant
y = 3;
result = (4294967295 - y)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3371: RHS constant
x = 4294967295;
result = (x - 3)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3372: both arguments variables
x = 4294967295;
y = -3;
result = (x - y);
check = 4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3373: both arguments constants
result = (4294967295 - -3)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3374: LHS constant
y = -3;
result = (4294967295 - y)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3375: RHS constant
x = 4294967295;
result = (x - -3)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3376: both arguments variables
x = 4294967295;
y = 4;
result = (x - y);
check = 4294967291;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3377: both arguments constants
result = (4294967295 - 4)
check = 4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3378: LHS constant
y = 4;
result = (4294967295 - y)
check = 4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3379: RHS constant
x = 4294967295;
result = (x - 4)
check = 4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3380: both arguments variables
x = 4294967295;
y = -4;
result = (x - y);
check = 4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3381: both arguments constants
result = (4294967295 - -4)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3382: LHS constant
y = -4;
result = (4294967295 - y)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3383: RHS constant
x = 4294967295;
result = (x - -4)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3384: both arguments variables
x = 4294967295;
y = 8;
result = (x - y);
check = 4294967287;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3385: both arguments constants
result = (4294967295 - 8)
check = 4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3386: LHS constant
y = 8;
result = (4294967295 - y)
check = 4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3387: RHS constant
x = 4294967295;
result = (x - 8)
check = 4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3388: both arguments variables
x = 4294967295;
y = -8;
result = (x - y);
check = 4294967303;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3389: both arguments constants
result = (4294967295 - -8)
check = 4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3390: LHS constant
y = -8;
result = (4294967295 - y)
check = 4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3391: RHS constant
x = 4294967295;
result = (x - -8)
check = 4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3392: both arguments variables
x = 4294967295;
y = 1073741822;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3393: both arguments constants
result = (4294967295 - 1073741822)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3394: LHS constant
y = 1073741822;
result = (4294967295 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3395: RHS constant
x = 4294967295;
result = (x - 1073741822)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3396: both arguments variables
x = 4294967295;
y = 1073741823;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3397: both arguments constants
result = (4294967295 - 1073741823)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3398: LHS constant
y = 1073741823;
result = (4294967295 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3399: RHS constant
x = 4294967295;
result = (x - 1073741823)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test34() {
var x;
var y;
var result;
var check;
// Test 3400: both arguments variables
x = 4294967295;
y = 1073741824;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3401: both arguments constants
result = (4294967295 - 1073741824)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3402: LHS constant
y = 1073741824;
result = (4294967295 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3403: RHS constant
x = 4294967295;
result = (x - 1073741824)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3404: both arguments variables
x = 4294967295;
y = 1073741825;
result = (x - y);
check = 3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3405: both arguments constants
result = (4294967295 - 1073741825)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3406: LHS constant
y = 1073741825;
result = (4294967295 - y)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3407: RHS constant
x = 4294967295;
result = (x - 1073741825)
check = 3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3408: both arguments variables
x = 4294967295;
y = -1073741823;
result = (x - y);
check = 5368709118;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3409: both arguments constants
result = (4294967295 - -1073741823)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3410: LHS constant
y = -1073741823;
result = (4294967295 - y)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3411: RHS constant
x = 4294967295;
result = (x - -1073741823)
check = 5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3412: both arguments variables
x = 4294967295;
y = (-0x3fffffff-1);
result = (x - y);
check = 5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3413: both arguments constants
result = (4294967295 - (-0x3fffffff-1))
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3414: LHS constant
y = (-0x3fffffff-1);
result = (4294967295 - y)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3415: RHS constant
x = 4294967295;
result = (x - (-0x3fffffff-1))
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3416: both arguments variables
x = 4294967295;
y = -1073741825;
result = (x - y);
check = 5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3417: both arguments constants
result = (4294967295 - -1073741825)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3418: LHS constant
y = -1073741825;
result = (4294967295 - y)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3419: RHS constant
x = 4294967295;
result = (x - -1073741825)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3420: both arguments variables
x = 4294967295;
y = -1073741826;
result = (x - y);
check = 5368709121;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3421: both arguments constants
result = (4294967295 - -1073741826)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3422: LHS constant
y = -1073741826;
result = (4294967295 - y)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3423: RHS constant
x = 4294967295;
result = (x - -1073741826)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3424: both arguments variables
x = 4294967295;
y = 2147483646;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3425: both arguments constants
result = (4294967295 - 2147483646)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3426: LHS constant
y = 2147483646;
result = (4294967295 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3427: RHS constant
x = 4294967295;
result = (x - 2147483646)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3428: both arguments variables
x = 4294967295;
y = 2147483647;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3429: both arguments constants
result = (4294967295 - 2147483647)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3430: LHS constant
y = 2147483647;
result = (4294967295 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3431: RHS constant
x = 4294967295;
result = (x - 2147483647)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3432: both arguments variables
x = 4294967295;
y = 2147483648;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3433: both arguments constants
result = (4294967295 - 2147483648)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3434: LHS constant
y = 2147483648;
result = (4294967295 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3435: RHS constant
x = 4294967295;
result = (x - 2147483648)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3436: both arguments variables
x = 4294967295;
y = 2147483649;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3437: both arguments constants
result = (4294967295 - 2147483649)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3438: LHS constant
y = 2147483649;
result = (4294967295 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3439: RHS constant
x = 4294967295;
result = (x - 2147483649)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3440: both arguments variables
x = 4294967295;
y = -2147483647;
result = (x - y);
check = 6442450942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3441: both arguments constants
result = (4294967295 - -2147483647)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3442: LHS constant
y = -2147483647;
result = (4294967295 - y)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3443: RHS constant
x = 4294967295;
result = (x - -2147483647)
check = 6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3444: both arguments variables
x = 4294967295;
y = -2147483648;
result = (x - y);
check = 6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3445: both arguments constants
result = (4294967295 - -2147483648)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3446: LHS constant
y = -2147483648;
result = (4294967295 - y)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3447: RHS constant
x = 4294967295;
result = (x - -2147483648)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3448: both arguments variables
x = 4294967295;
y = -2147483649;
result = (x - y);
check = 6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3449: both arguments constants
result = (4294967295 - -2147483649)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3450: LHS constant
y = -2147483649;
result = (4294967295 - y)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3451: RHS constant
x = 4294967295;
result = (x - -2147483649)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3452: both arguments variables
x = 4294967295;
y = -2147483650;
result = (x - y);
check = 6442450945;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3453: both arguments constants
result = (4294967295 - -2147483650)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3454: LHS constant
y = -2147483650;
result = (4294967295 - y)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3455: RHS constant
x = 4294967295;
result = (x - -2147483650)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3456: both arguments variables
x = 4294967295;
y = 4294967295;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3457: both arguments constants
result = (4294967295 - 4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3458: LHS constant
y = 4294967295;
result = (4294967295 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3459: RHS constant
x = 4294967295;
result = (x - 4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3460: both arguments variables
x = 4294967295;
y = 4294967296;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3461: both arguments constants
result = (4294967295 - 4294967296)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3462: LHS constant
y = 4294967296;
result = (4294967295 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3463: RHS constant
x = 4294967295;
result = (x - 4294967296)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3464: both arguments variables
x = 4294967295;
y = -4294967295;
result = (x - y);
check = 8589934590;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3465: both arguments constants
result = (4294967295 - -4294967295)
check = 8589934590
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3466: LHS constant
y = -4294967295;
result = (4294967295 - y)
check = 8589934590
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3467: RHS constant
x = 4294967295;
result = (x - -4294967295)
check = 8589934590
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3468: both arguments variables
x = 4294967295;
y = -4294967296;
result = (x - y);
check = 8589934591;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3469: both arguments constants
result = (4294967295 - -4294967296)
check = 8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3470: LHS constant
y = -4294967296;
result = (4294967295 - y)
check = 8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3471: RHS constant
x = 4294967295;
result = (x - -4294967296)
check = 8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3472: both arguments variables
x = 4294967296;
y = 0;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3473: both arguments constants
result = (4294967296 - 0)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3474: LHS constant
y = 0;
result = (4294967296 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3475: RHS constant
x = 4294967296;
result = (x - 0)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3476: both arguments variables
x = 4294967296;
y = 1;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3477: both arguments constants
result = (4294967296 - 1)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3478: LHS constant
y = 1;
result = (4294967296 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3479: RHS constant
x = 4294967296;
result = (x - 1)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3480: both arguments variables
x = 4294967296;
y = -1;
result = (x - y);
check = 4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3481: both arguments constants
result = (4294967296 - -1)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3482: LHS constant
y = -1;
result = (4294967296 - y)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3483: RHS constant
x = 4294967296;
result = (x - -1)
check = 4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3484: both arguments variables
x = 4294967296;
y = 2;
result = (x - y);
check = 4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3485: both arguments constants
result = (4294967296 - 2)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3486: LHS constant
y = 2;
result = (4294967296 - y)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3487: RHS constant
x = 4294967296;
result = (x - 2)
check = 4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3488: both arguments variables
x = 4294967296;
y = -2;
result = (x - y);
check = 4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3489: both arguments constants
result = (4294967296 - -2)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3490: LHS constant
y = -2;
result = (4294967296 - y)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3491: RHS constant
x = 4294967296;
result = (x - -2)
check = 4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3492: both arguments variables
x = 4294967296;
y = 3;
result = (x - y);
check = 4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3493: both arguments constants
result = (4294967296 - 3)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3494: LHS constant
y = 3;
result = (4294967296 - y)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3495: RHS constant
x = 4294967296;
result = (x - 3)
check = 4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3496: both arguments variables
x = 4294967296;
y = -3;
result = (x - y);
check = 4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3497: both arguments constants
result = (4294967296 - -3)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3498: LHS constant
y = -3;
result = (4294967296 - y)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3499: RHS constant
x = 4294967296;
result = (x - -3)
check = 4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test35() {
var x;
var y;
var result;
var check;
// Test 3500: both arguments variables
x = 4294967296;
y = 4;
result = (x - y);
check = 4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3501: both arguments constants
result = (4294967296 - 4)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3502: LHS constant
y = 4;
result = (4294967296 - y)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3503: RHS constant
x = 4294967296;
result = (x - 4)
check = 4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3504: both arguments variables
x = 4294967296;
y = -4;
result = (x - y);
check = 4294967300;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3505: both arguments constants
result = (4294967296 - -4)
check = 4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3506: LHS constant
y = -4;
result = (4294967296 - y)
check = 4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3507: RHS constant
x = 4294967296;
result = (x - -4)
check = 4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3508: both arguments variables
x = 4294967296;
y = 8;
result = (x - y);
check = 4294967288;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3509: both arguments constants
result = (4294967296 - 8)
check = 4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3510: LHS constant
y = 8;
result = (4294967296 - y)
check = 4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3511: RHS constant
x = 4294967296;
result = (x - 8)
check = 4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3512: both arguments variables
x = 4294967296;
y = -8;
result = (x - y);
check = 4294967304;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3513: both arguments constants
result = (4294967296 - -8)
check = 4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3514: LHS constant
y = -8;
result = (4294967296 - y)
check = 4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3515: RHS constant
x = 4294967296;
result = (x - -8)
check = 4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3516: both arguments variables
x = 4294967296;
y = 1073741822;
result = (x - y);
check = 3221225474;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3517: both arguments constants
result = (4294967296 - 1073741822)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3518: LHS constant
y = 1073741822;
result = (4294967296 - y)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3519: RHS constant
x = 4294967296;
result = (x - 1073741822)
check = 3221225474
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3520: both arguments variables
x = 4294967296;
y = 1073741823;
result = (x - y);
check = 3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3521: both arguments constants
result = (4294967296 - 1073741823)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3522: LHS constant
y = 1073741823;
result = (4294967296 - y)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3523: RHS constant
x = 4294967296;
result = (x - 1073741823)
check = 3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3524: both arguments variables
x = 4294967296;
y = 1073741824;
result = (x - y);
check = 3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3525: both arguments constants
result = (4294967296 - 1073741824)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3526: LHS constant
y = 1073741824;
result = (4294967296 - y)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3527: RHS constant
x = 4294967296;
result = (x - 1073741824)
check = 3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3528: both arguments variables
x = 4294967296;
y = 1073741825;
result = (x - y);
check = 3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3529: both arguments constants
result = (4294967296 - 1073741825)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3530: LHS constant
y = 1073741825;
result = (4294967296 - y)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3531: RHS constant
x = 4294967296;
result = (x - 1073741825)
check = 3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3532: both arguments variables
x = 4294967296;
y = -1073741823;
result = (x - y);
check = 5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3533: both arguments constants
result = (4294967296 - -1073741823)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3534: LHS constant
y = -1073741823;
result = (4294967296 - y)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3535: RHS constant
x = 4294967296;
result = (x - -1073741823)
check = 5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3536: both arguments variables
x = 4294967296;
y = (-0x3fffffff-1);
result = (x - y);
check = 5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3537: both arguments constants
result = (4294967296 - (-0x3fffffff-1))
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3538: LHS constant
y = (-0x3fffffff-1);
result = (4294967296 - y)
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3539: RHS constant
x = 4294967296;
result = (x - (-0x3fffffff-1))
check = 5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3540: both arguments variables
x = 4294967296;
y = -1073741825;
result = (x - y);
check = 5368709121;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3541: both arguments constants
result = (4294967296 - -1073741825)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3542: LHS constant
y = -1073741825;
result = (4294967296 - y)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3543: RHS constant
x = 4294967296;
result = (x - -1073741825)
check = 5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3544: both arguments variables
x = 4294967296;
y = -1073741826;
result = (x - y);
check = 5368709122;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3545: both arguments constants
result = (4294967296 - -1073741826)
check = 5368709122
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3546: LHS constant
y = -1073741826;
result = (4294967296 - y)
check = 5368709122
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3547: RHS constant
x = 4294967296;
result = (x - -1073741826)
check = 5368709122
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3548: both arguments variables
x = 4294967296;
y = 2147483646;
result = (x - y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3549: both arguments constants
result = (4294967296 - 2147483646)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3550: LHS constant
y = 2147483646;
result = (4294967296 - y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3551: RHS constant
x = 4294967296;
result = (x - 2147483646)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3552: both arguments variables
x = 4294967296;
y = 2147483647;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3553: both arguments constants
result = (4294967296 - 2147483647)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3554: LHS constant
y = 2147483647;
result = (4294967296 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3555: RHS constant
x = 4294967296;
result = (x - 2147483647)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3556: both arguments variables
x = 4294967296;
y = 2147483648;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3557: both arguments constants
result = (4294967296 - 2147483648)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3558: LHS constant
y = 2147483648;
result = (4294967296 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3559: RHS constant
x = 4294967296;
result = (x - 2147483648)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3560: both arguments variables
x = 4294967296;
y = 2147483649;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3561: both arguments constants
result = (4294967296 - 2147483649)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3562: LHS constant
y = 2147483649;
result = (4294967296 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3563: RHS constant
x = 4294967296;
result = (x - 2147483649)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3564: both arguments variables
x = 4294967296;
y = -2147483647;
result = (x - y);
check = 6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3565: both arguments constants
result = (4294967296 - -2147483647)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3566: LHS constant
y = -2147483647;
result = (4294967296 - y)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3567: RHS constant
x = 4294967296;
result = (x - -2147483647)
check = 6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3568: both arguments variables
x = 4294967296;
y = -2147483648;
result = (x - y);
check = 6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3569: both arguments constants
result = (4294967296 - -2147483648)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3570: LHS constant
y = -2147483648;
result = (4294967296 - y)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3571: RHS constant
x = 4294967296;
result = (x - -2147483648)
check = 6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3572: both arguments variables
x = 4294967296;
y = -2147483649;
result = (x - y);
check = 6442450945;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3573: both arguments constants
result = (4294967296 - -2147483649)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3574: LHS constant
y = -2147483649;
result = (4294967296 - y)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3575: RHS constant
x = 4294967296;
result = (x - -2147483649)
check = 6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3576: both arguments variables
x = 4294967296;
y = -2147483650;
result = (x - y);
check = 6442450946;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3577: both arguments constants
result = (4294967296 - -2147483650)
check = 6442450946
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3578: LHS constant
y = -2147483650;
result = (4294967296 - y)
check = 6442450946
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3579: RHS constant
x = 4294967296;
result = (x - -2147483650)
check = 6442450946
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3580: both arguments variables
x = 4294967296;
y = 4294967295;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3581: both arguments constants
result = (4294967296 - 4294967295)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3582: LHS constant
y = 4294967295;
result = (4294967296 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3583: RHS constant
x = 4294967296;
result = (x - 4294967295)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3584: both arguments variables
x = 4294967296;
y = 4294967296;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3585: both arguments constants
result = (4294967296 - 4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3586: LHS constant
y = 4294967296;
result = (4294967296 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3587: RHS constant
x = 4294967296;
result = (x - 4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3588: both arguments variables
x = 4294967296;
y = -4294967295;
result = (x - y);
check = 8589934591;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3589: both arguments constants
result = (4294967296 - -4294967295)
check = 8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3590: LHS constant
y = -4294967295;
result = (4294967296 - y)
check = 8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3591: RHS constant
x = 4294967296;
result = (x - -4294967295)
check = 8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3592: both arguments variables
x = 4294967296;
y = -4294967296;
result = (x - y);
check = 8589934592;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3593: both arguments constants
result = (4294967296 - -4294967296)
check = 8589934592
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3594: LHS constant
y = -4294967296;
result = (4294967296 - y)
check = 8589934592
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3595: RHS constant
x = 4294967296;
result = (x - -4294967296)
check = 8589934592
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3596: both arguments variables
x = -4294967295;
y = 0;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3597: both arguments constants
result = (-4294967295 - 0)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3598: LHS constant
y = 0;
result = (-4294967295 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3599: RHS constant
x = -4294967295;
result = (x - 0)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test36() {
var x;
var y;
var result;
var check;
// Test 3600: both arguments variables
x = -4294967295;
y = 1;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3601: both arguments constants
result = (-4294967295 - 1)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3602: LHS constant
y = 1;
result = (-4294967295 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3603: RHS constant
x = -4294967295;
result = (x - 1)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3604: both arguments variables
x = -4294967295;
y = -1;
result = (x - y);
check = -4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3605: both arguments constants
result = (-4294967295 - -1)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3606: LHS constant
y = -1;
result = (-4294967295 - y)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3607: RHS constant
x = -4294967295;
result = (x - -1)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3608: both arguments variables
x = -4294967295;
y = 2;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3609: both arguments constants
result = (-4294967295 - 2)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3610: LHS constant
y = 2;
result = (-4294967295 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3611: RHS constant
x = -4294967295;
result = (x - 2)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3612: both arguments variables
x = -4294967295;
y = -2;
result = (x - y);
check = -4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3613: both arguments constants
result = (-4294967295 - -2)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3614: LHS constant
y = -2;
result = (-4294967295 - y)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3615: RHS constant
x = -4294967295;
result = (x - -2)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3616: both arguments variables
x = -4294967295;
y = 3;
result = (x - y);
check = -4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3617: both arguments constants
result = (-4294967295 - 3)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3618: LHS constant
y = 3;
result = (-4294967295 - y)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3619: RHS constant
x = -4294967295;
result = (x - 3)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3620: both arguments variables
x = -4294967295;
y = -3;
result = (x - y);
check = -4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3621: both arguments constants
result = (-4294967295 - -3)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3622: LHS constant
y = -3;
result = (-4294967295 - y)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3623: RHS constant
x = -4294967295;
result = (x - -3)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3624: both arguments variables
x = -4294967295;
y = 4;
result = (x - y);
check = -4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3625: both arguments constants
result = (-4294967295 - 4)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3626: LHS constant
y = 4;
result = (-4294967295 - y)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3627: RHS constant
x = -4294967295;
result = (x - 4)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3628: both arguments variables
x = -4294967295;
y = -4;
result = (x - y);
check = -4294967291;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3629: both arguments constants
result = (-4294967295 - -4)
check = -4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3630: LHS constant
y = -4;
result = (-4294967295 - y)
check = -4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3631: RHS constant
x = -4294967295;
result = (x - -4)
check = -4294967291
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3632: both arguments variables
x = -4294967295;
y = 8;
result = (x - y);
check = -4294967303;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3633: both arguments constants
result = (-4294967295 - 8)
check = -4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3634: LHS constant
y = 8;
result = (-4294967295 - y)
check = -4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3635: RHS constant
x = -4294967295;
result = (x - 8)
check = -4294967303
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3636: both arguments variables
x = -4294967295;
y = -8;
result = (x - y);
check = -4294967287;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3637: both arguments constants
result = (-4294967295 - -8)
check = -4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3638: LHS constant
y = -8;
result = (-4294967295 - y)
check = -4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3639: RHS constant
x = -4294967295;
result = (x - -8)
check = -4294967287
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3640: both arguments variables
x = -4294967295;
y = 1073741822;
result = (x - y);
check = -5368709117;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3641: both arguments constants
result = (-4294967295 - 1073741822)
check = -5368709117
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3642: LHS constant
y = 1073741822;
result = (-4294967295 - y)
check = -5368709117
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3643: RHS constant
x = -4294967295;
result = (x - 1073741822)
check = -5368709117
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3644: both arguments variables
x = -4294967295;
y = 1073741823;
result = (x - y);
check = -5368709118;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3645: both arguments constants
result = (-4294967295 - 1073741823)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3646: LHS constant
y = 1073741823;
result = (-4294967295 - y)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3647: RHS constant
x = -4294967295;
result = (x - 1073741823)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3648: both arguments variables
x = -4294967295;
y = 1073741824;
result = (x - y);
check = -5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3649: both arguments constants
result = (-4294967295 - 1073741824)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3650: LHS constant
y = 1073741824;
result = (-4294967295 - y)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3651: RHS constant
x = -4294967295;
result = (x - 1073741824)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3652: both arguments variables
x = -4294967295;
y = 1073741825;
result = (x - y);
check = -5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3653: both arguments constants
result = (-4294967295 - 1073741825)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3654: LHS constant
y = 1073741825;
result = (-4294967295 - y)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3655: RHS constant
x = -4294967295;
result = (x - 1073741825)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3656: both arguments variables
x = -4294967295;
y = -1073741823;
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3657: both arguments constants
result = (-4294967295 - -1073741823)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3658: LHS constant
y = -1073741823;
result = (-4294967295 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3659: RHS constant
x = -4294967295;
result = (x - -1073741823)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3660: both arguments variables
x = -4294967295;
y = (-0x3fffffff-1);
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3661: both arguments constants
result = (-4294967295 - (-0x3fffffff-1))
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3662: LHS constant
y = (-0x3fffffff-1);
result = (-4294967295 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3663: RHS constant
x = -4294967295;
result = (x - (-0x3fffffff-1))
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3664: both arguments variables
x = -4294967295;
y = -1073741825;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3665: both arguments constants
result = (-4294967295 - -1073741825)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3666: LHS constant
y = -1073741825;
result = (-4294967295 - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3667: RHS constant
x = -4294967295;
result = (x - -1073741825)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3668: both arguments variables
x = -4294967295;
y = -1073741826;
result = (x - y);
check = -3221225469;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3669: both arguments constants
result = (-4294967295 - -1073741826)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3670: LHS constant
y = -1073741826;
result = (-4294967295 - y)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3671: RHS constant
x = -4294967295;
result = (x - -1073741826)
check = -3221225469
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3672: both arguments variables
x = -4294967295;
y = 2147483646;
result = (x - y);
check = -6442450941;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3673: both arguments constants
result = (-4294967295 - 2147483646)
check = -6442450941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3674: LHS constant
y = 2147483646;
result = (-4294967295 - y)
check = -6442450941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3675: RHS constant
x = -4294967295;
result = (x - 2147483646)
check = -6442450941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3676: both arguments variables
x = -4294967295;
y = 2147483647;
result = (x - y);
check = -6442450942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3677: both arguments constants
result = (-4294967295 - 2147483647)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3678: LHS constant
y = 2147483647;
result = (-4294967295 - y)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3679: RHS constant
x = -4294967295;
result = (x - 2147483647)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3680: both arguments variables
x = -4294967295;
y = 2147483648;
result = (x - y);
check = -6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3681: both arguments constants
result = (-4294967295 - 2147483648)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3682: LHS constant
y = 2147483648;
result = (-4294967295 - y)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3683: RHS constant
x = -4294967295;
result = (x - 2147483648)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3684: both arguments variables
x = -4294967295;
y = 2147483649;
result = (x - y);
check = -6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3685: both arguments constants
result = (-4294967295 - 2147483649)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3686: LHS constant
y = 2147483649;
result = (-4294967295 - y)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3687: RHS constant
x = -4294967295;
result = (x - 2147483649)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3688: both arguments variables
x = -4294967295;
y = -2147483647;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3689: both arguments constants
result = (-4294967295 - -2147483647)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3690: LHS constant
y = -2147483647;
result = (-4294967295 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3691: RHS constant
x = -4294967295;
result = (x - -2147483647)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3692: both arguments variables
x = -4294967295;
y = -2147483648;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3693: both arguments constants
result = (-4294967295 - -2147483648)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3694: LHS constant
y = -2147483648;
result = (-4294967295 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3695: RHS constant
x = -4294967295;
result = (x - -2147483648)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3696: both arguments variables
x = -4294967295;
y = -2147483649;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3697: both arguments constants
result = (-4294967295 - -2147483649)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3698: LHS constant
y = -2147483649;
result = (-4294967295 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3699: RHS constant
x = -4294967295;
result = (x - -2147483649)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test37() {
var x;
var y;
var result;
var check;
// Test 3700: both arguments variables
x = -4294967295;
y = -2147483650;
result = (x - y);
check = -2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3701: both arguments constants
result = (-4294967295 - -2147483650)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3702: LHS constant
y = -2147483650;
result = (-4294967295 - y)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3703: RHS constant
x = -4294967295;
result = (x - -2147483650)
check = -2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3704: both arguments variables
x = -4294967295;
y = 4294967295;
result = (x - y);
check = -8589934590;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3705: both arguments constants
result = (-4294967295 - 4294967295)
check = -8589934590
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3706: LHS constant
y = 4294967295;
result = (-4294967295 - y)
check = -8589934590
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3707: RHS constant
x = -4294967295;
result = (x - 4294967295)
check = -8589934590
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3708: both arguments variables
x = -4294967295;
y = 4294967296;
result = (x - y);
check = -8589934591;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3709: both arguments constants
result = (-4294967295 - 4294967296)
check = -8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3710: LHS constant
y = 4294967296;
result = (-4294967295 - y)
check = -8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3711: RHS constant
x = -4294967295;
result = (x - 4294967296)
check = -8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3712: both arguments variables
x = -4294967295;
y = -4294967295;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3713: both arguments constants
result = (-4294967295 - -4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3714: LHS constant
y = -4294967295;
result = (-4294967295 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3715: RHS constant
x = -4294967295;
result = (x - -4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3716: both arguments variables
x = -4294967295;
y = -4294967296;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3717: both arguments constants
result = (-4294967295 - -4294967296)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3718: LHS constant
y = -4294967296;
result = (-4294967295 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3719: RHS constant
x = -4294967295;
result = (x - -4294967296)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3720: both arguments variables
x = -4294967296;
y = 0;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3721: both arguments constants
result = (-4294967296 - 0)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3722: LHS constant
y = 0;
result = (-4294967296 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3723: RHS constant
x = -4294967296;
result = (x - 0)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3724: both arguments variables
x = -4294967296;
y = 1;
result = (x - y);
check = -4294967297;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3725: both arguments constants
result = (-4294967296 - 1)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3726: LHS constant
y = 1;
result = (-4294967296 - y)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3727: RHS constant
x = -4294967296;
result = (x - 1)
check = -4294967297
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3728: both arguments variables
x = -4294967296;
y = -1;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3729: both arguments constants
result = (-4294967296 - -1)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3730: LHS constant
y = -1;
result = (-4294967296 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3731: RHS constant
x = -4294967296;
result = (x - -1)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3732: both arguments variables
x = -4294967296;
y = 2;
result = (x - y);
check = -4294967298;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3733: both arguments constants
result = (-4294967296 - 2)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3734: LHS constant
y = 2;
result = (-4294967296 - y)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3735: RHS constant
x = -4294967296;
result = (x - 2)
check = -4294967298
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3736: both arguments variables
x = -4294967296;
y = -2;
result = (x - y);
check = -4294967294;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3737: both arguments constants
result = (-4294967296 - -2)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3738: LHS constant
y = -2;
result = (-4294967296 - y)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3739: RHS constant
x = -4294967296;
result = (x - -2)
check = -4294967294
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3740: both arguments variables
x = -4294967296;
y = 3;
result = (x - y);
check = -4294967299;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3741: both arguments constants
result = (-4294967296 - 3)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3742: LHS constant
y = 3;
result = (-4294967296 - y)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3743: RHS constant
x = -4294967296;
result = (x - 3)
check = -4294967299
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3744: both arguments variables
x = -4294967296;
y = -3;
result = (x - y);
check = -4294967293;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3745: both arguments constants
result = (-4294967296 - -3)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3746: LHS constant
y = -3;
result = (-4294967296 - y)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3747: RHS constant
x = -4294967296;
result = (x - -3)
check = -4294967293
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3748: both arguments variables
x = -4294967296;
y = 4;
result = (x - y);
check = -4294967300;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3749: both arguments constants
result = (-4294967296 - 4)
check = -4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3750: LHS constant
y = 4;
result = (-4294967296 - y)
check = -4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3751: RHS constant
x = -4294967296;
result = (x - 4)
check = -4294967300
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3752: both arguments variables
x = -4294967296;
y = -4;
result = (x - y);
check = -4294967292;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3753: both arguments constants
result = (-4294967296 - -4)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3754: LHS constant
y = -4;
result = (-4294967296 - y)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3755: RHS constant
x = -4294967296;
result = (x - -4)
check = -4294967292
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3756: both arguments variables
x = -4294967296;
y = 8;
result = (x - y);
check = -4294967304;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3757: both arguments constants
result = (-4294967296 - 8)
check = -4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3758: LHS constant
y = 8;
result = (-4294967296 - y)
check = -4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3759: RHS constant
x = -4294967296;
result = (x - 8)
check = -4294967304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3760: both arguments variables
x = -4294967296;
y = -8;
result = (x - y);
check = -4294967288;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3761: both arguments constants
result = (-4294967296 - -8)
check = -4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3762: LHS constant
y = -8;
result = (-4294967296 - y)
check = -4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3763: RHS constant
x = -4294967296;
result = (x - -8)
check = -4294967288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3764: both arguments variables
x = -4294967296;
y = 1073741822;
result = (x - y);
check = -5368709118;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3765: both arguments constants
result = (-4294967296 - 1073741822)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3766: LHS constant
y = 1073741822;
result = (-4294967296 - y)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3767: RHS constant
x = -4294967296;
result = (x - 1073741822)
check = -5368709118
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3768: both arguments variables
x = -4294967296;
y = 1073741823;
result = (x - y);
check = -5368709119;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3769: both arguments constants
result = (-4294967296 - 1073741823)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3770: LHS constant
y = 1073741823;
result = (-4294967296 - y)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3771: RHS constant
x = -4294967296;
result = (x - 1073741823)
check = -5368709119
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3772: both arguments variables
x = -4294967296;
y = 1073741824;
result = (x - y);
check = -5368709120;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3773: both arguments constants
result = (-4294967296 - 1073741824)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3774: LHS constant
y = 1073741824;
result = (-4294967296 - y)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3775: RHS constant
x = -4294967296;
result = (x - 1073741824)
check = -5368709120
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3776: both arguments variables
x = -4294967296;
y = 1073741825;
result = (x - y);
check = -5368709121;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3777: both arguments constants
result = (-4294967296 - 1073741825)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3778: LHS constant
y = 1073741825;
result = (-4294967296 - y)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3779: RHS constant
x = -4294967296;
result = (x - 1073741825)
check = -5368709121
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3780: both arguments variables
x = -4294967296;
y = -1073741823;
result = (x - y);
check = -3221225473;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3781: both arguments constants
result = (-4294967296 - -1073741823)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3782: LHS constant
y = -1073741823;
result = (-4294967296 - y)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3783: RHS constant
x = -4294967296;
result = (x - -1073741823)
check = -3221225473
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3784: both arguments variables
x = -4294967296;
y = (-0x3fffffff-1);
result = (x - y);
check = -3221225472;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3785: both arguments constants
result = (-4294967296 - (-0x3fffffff-1))
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3786: LHS constant
y = (-0x3fffffff-1);
result = (-4294967296 - y)
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3787: RHS constant
x = -4294967296;
result = (x - (-0x3fffffff-1))
check = -3221225472
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3788: both arguments variables
x = -4294967296;
y = -1073741825;
result = (x - y);
check = -3221225471;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3789: both arguments constants
result = (-4294967296 - -1073741825)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3790: LHS constant
y = -1073741825;
result = (-4294967296 - y)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3791: RHS constant
x = -4294967296;
result = (x - -1073741825)
check = -3221225471
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3792: both arguments variables
x = -4294967296;
y = -1073741826;
result = (x - y);
check = -3221225470;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3793: both arguments constants
result = (-4294967296 - -1073741826)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3794: LHS constant
y = -1073741826;
result = (-4294967296 - y)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3795: RHS constant
x = -4294967296;
result = (x - -1073741826)
check = -3221225470
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3796: both arguments variables
x = -4294967296;
y = 2147483646;
result = (x - y);
check = -6442450942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3797: both arguments constants
result = (-4294967296 - 2147483646)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3798: LHS constant
y = 2147483646;
result = (-4294967296 - y)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3799: RHS constant
x = -4294967296;
result = (x - 2147483646)
check = -6442450942
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test38() {
var x;
var y;
var result;
var check;
// Test 3800: both arguments variables
x = -4294967296;
y = 2147483647;
result = (x - y);
check = -6442450943;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3801: both arguments constants
result = (-4294967296 - 2147483647)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3802: LHS constant
y = 2147483647;
result = (-4294967296 - y)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3803: RHS constant
x = -4294967296;
result = (x - 2147483647)
check = -6442450943
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3804: both arguments variables
x = -4294967296;
y = 2147483648;
result = (x - y);
check = -6442450944;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3805: both arguments constants
result = (-4294967296 - 2147483648)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3806: LHS constant
y = 2147483648;
result = (-4294967296 - y)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3807: RHS constant
x = -4294967296;
result = (x - 2147483648)
check = -6442450944
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3808: both arguments variables
x = -4294967296;
y = 2147483649;
result = (x - y);
check = -6442450945;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3809: both arguments constants
result = (-4294967296 - 2147483649)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3810: LHS constant
y = 2147483649;
result = (-4294967296 - y)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3811: RHS constant
x = -4294967296;
result = (x - 2147483649)
check = -6442450945
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3812: both arguments variables
x = -4294967296;
y = -2147483647;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3813: both arguments constants
result = (-4294967296 - -2147483647)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3814: LHS constant
y = -2147483647;
result = (-4294967296 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3815: RHS constant
x = -4294967296;
result = (x - -2147483647)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3816: both arguments variables
x = -4294967296;
y = -2147483648;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3817: both arguments constants
result = (-4294967296 - -2147483648)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3818: LHS constant
y = -2147483648;
result = (-4294967296 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3819: RHS constant
x = -4294967296;
result = (x - -2147483648)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3820: both arguments variables
x = -4294967296;
y = -2147483649;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3821: both arguments constants
result = (-4294967296 - -2147483649)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3822: LHS constant
y = -2147483649;
result = (-4294967296 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3823: RHS constant
x = -4294967296;
result = (x - -2147483649)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3824: both arguments variables
x = -4294967296;
y = -2147483650;
result = (x - y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3825: both arguments constants
result = (-4294967296 - -2147483650)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3826: LHS constant
y = -2147483650;
result = (-4294967296 - y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3827: RHS constant
x = -4294967296;
result = (x - -2147483650)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3828: both arguments variables
x = -4294967296;
y = 4294967295;
result = (x - y);
check = -8589934591;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3829: both arguments constants
result = (-4294967296 - 4294967295)
check = -8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3830: LHS constant
y = 4294967295;
result = (-4294967296 - y)
check = -8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3831: RHS constant
x = -4294967296;
result = (x - 4294967295)
check = -8589934591
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3832: both arguments variables
x = -4294967296;
y = 4294967296;
result = (x - y);
check = -8589934592;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3833: both arguments constants
result = (-4294967296 - 4294967296)
check = -8589934592
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3834: LHS constant
y = 4294967296;
result = (-4294967296 - y)
check = -8589934592
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3835: RHS constant
x = -4294967296;
result = (x - 4294967296)
check = -8589934592
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3836: both arguments variables
x = -4294967296;
y = -4294967295;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3837: both arguments constants
result = (-4294967296 - -4294967295)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3838: LHS constant
y = -4294967295;
result = (-4294967296 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3839: RHS constant
x = -4294967296;
result = (x - -4294967295)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3840: both arguments variables
x = -4294967296;
y = -4294967296;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3841: both arguments constants
result = (-4294967296 - -4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3842: LHS constant
y = -4294967296;
result = (-4294967296 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3843: RHS constant
x = -4294967296;
result = (x - -4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3844: both arguments variables
x = 248281107940391;
y = 248281107940391;
result = (x - y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3845: both arguments constants
result = (248281107940391 - 248281107940391)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3846: LHS constant
y = 248281107940391;
result = (248281107940391 - y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3847: RHS constant
x = 248281107940391;
result = (x - 248281107940391)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3848: both arguments variables
x = 248281107940391;
y = 248281107940390;
result = (x - y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3849: both arguments constants
result = (248281107940391 - 248281107940390)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3850: LHS constant
y = 248281107940390;
result = (248281107940391 - y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3851: RHS constant
x = 248281107940391;
result = (x - 248281107940390)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3852: both arguments variables
x = 248281107940391;
y = 248281107940392;
result = (x - y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3853: both arguments constants
result = (248281107940391 - 248281107940392)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3854: LHS constant
y = 248281107940392;
result = (248281107940391 - y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3855: RHS constant
x = 248281107940391;
result = (x - 248281107940392)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3856: both arguments variables
x = 248281107940391;
y = 248281107940389;
result = (x - y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3857: both arguments constants
result = (248281107940391 - 248281107940389)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3858: LHS constant
y = 248281107940389;
result = (248281107940391 - y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3859: RHS constant
x = 248281107940391;
result = (x - 248281107940389)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3860: both arguments variables
x = 248281107940391;
y = 248281107940393;
result = (x - y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3861: both arguments constants
result = (248281107940391 - 248281107940393)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3862: LHS constant
y = 248281107940393;
result = (248281107940391 - y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3863: RHS constant
x = 248281107940391;
result = (x - 248281107940393)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3864: both arguments variables
x = 248281107940391;
y = 248281107940388;
result = (x - y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3865: both arguments constants
result = (248281107940391 - 248281107940388)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3866: LHS constant
y = 248281107940388;
result = (248281107940391 - y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3867: RHS constant
x = 248281107940391;
result = (x - 248281107940388)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3868: both arguments variables
x = 248281107940391;
y = 248281107940394;
result = (x - y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3869: both arguments constants
result = (248281107940391 - 248281107940394)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3870: LHS constant
y = 248281107940394;
result = (248281107940391 - y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3871: RHS constant
x = 248281107940391;
result = (x - 248281107940394)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3872: both arguments variables
x = 248281107940391;
y = 248281107940387;
result = (x - y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3873: both arguments constants
result = (248281107940391 - 248281107940387)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3874: LHS constant
y = 248281107940387;
result = (248281107940391 - y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3875: RHS constant
x = 248281107940391;
result = (x - 248281107940387)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3876: both arguments variables
x = 248281107940391;
y = 248281107940395;
result = (x - y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3877: both arguments constants
result = (248281107940391 - 248281107940395)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3878: LHS constant
y = 248281107940395;
result = (248281107940391 - y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3879: RHS constant
x = 248281107940391;
result = (x - 248281107940395)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3880: both arguments variables
x = 248281107940391;
y = 248281107940383;
result = (x - y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3881: both arguments constants
result = (248281107940391 - 248281107940383)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3882: LHS constant
y = 248281107940383;
result = (248281107940391 - y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3883: RHS constant
x = 248281107940391;
result = (x - 248281107940383)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3884: both arguments variables
x = 248281107940391;
y = 248281107940399;
result = (x - y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3885: both arguments constants
result = (248281107940391 - 248281107940399)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3886: LHS constant
y = 248281107940399;
result = (248281107940391 - y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3887: RHS constant
x = 248281107940391;
result = (x - 248281107940399)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3888: both arguments variables
x = 248281107940391;
y = 248280034198569;
result = (x - y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3889: both arguments constants
result = (248281107940391 - 248280034198569)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3890: LHS constant
y = 248280034198569;
result = (248281107940391 - y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3891: RHS constant
x = 248281107940391;
result = (x - 248280034198569)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3892: both arguments variables
x = 248281107940391;
y = 248280034198568;
result = (x - y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3893: both arguments constants
result = (248281107940391 - 248280034198568)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3894: LHS constant
y = 248280034198568;
result = (248281107940391 - y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3895: RHS constant
x = 248281107940391;
result = (x - 248280034198568)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3896: both arguments variables
x = 248281107940391;
y = 248280034198567;
result = (x - y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3897: both arguments constants
result = (248281107940391 - 248280034198567)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3898: LHS constant
y = 248280034198567;
result = (248281107940391 - y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3899: RHS constant
x = 248281107940391;
result = (x - 248280034198567)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test39() {
var x;
var y;
var result;
var check;
// Test 3900: both arguments variables
x = 248281107940391;
y = 248280034198566;
result = (x - y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3901: both arguments constants
result = (248281107940391 - 248280034198566)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3902: LHS constant
y = 248280034198566;
result = (248281107940391 - y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3903: RHS constant
x = 248281107940391;
result = (x - 248280034198566)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3904: both arguments variables
x = 248281107940391;
y = 248282181682214;
result = (x - y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3905: both arguments constants
result = (248281107940391 - 248282181682214)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3906: LHS constant
y = 248282181682214;
result = (248281107940391 - y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3907: RHS constant
x = 248281107940391;
result = (x - 248282181682214)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3908: both arguments variables
x = 248281107940391;
y = 248282181682215;
result = (x - y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3909: both arguments constants
result = (248281107940391 - 248282181682215)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3910: LHS constant
y = 248282181682215;
result = (248281107940391 - y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3911: RHS constant
x = 248281107940391;
result = (x - 248282181682215)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3912: both arguments variables
x = 248281107940391;
y = 248282181682216;
result = (x - y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3913: both arguments constants
result = (248281107940391 - 248282181682216)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3914: LHS constant
y = 248282181682216;
result = (248281107940391 - y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3915: RHS constant
x = 248281107940391;
result = (x - 248282181682216)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3916: both arguments variables
x = 248281107940391;
y = 248282181682217;
result = (x - y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3917: both arguments constants
result = (248281107940391 - 248282181682217)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3918: LHS constant
y = 248282181682217;
result = (248281107940391 - y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3919: RHS constant
x = 248281107940391;
result = (x - 248282181682217)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3920: both arguments variables
x = 248281107940391;
y = 248278960456745;
result = (x - y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3921: both arguments constants
result = (248281107940391 - 248278960456745)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3922: LHS constant
y = 248278960456745;
result = (248281107940391 - y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3923: RHS constant
x = 248281107940391;
result = (x - 248278960456745)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3924: both arguments variables
x = 248281107940391;
y = 248278960456744;
result = (x - y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3925: both arguments constants
result = (248281107940391 - 248278960456744)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3926: LHS constant
y = 248278960456744;
result = (248281107940391 - y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3927: RHS constant
x = 248281107940391;
result = (x - 248278960456744)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3928: both arguments variables
x = 248281107940391;
y = 248278960456743;
result = (x - y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3929: both arguments constants
result = (248281107940391 - 248278960456743)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3930: LHS constant
y = 248278960456743;
result = (248281107940391 - y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3931: RHS constant
x = 248281107940391;
result = (x - 248278960456743)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3932: both arguments variables
x = 248281107940391;
y = 248278960456742;
result = (x - y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3933: both arguments constants
result = (248281107940391 - 248278960456742)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3934: LHS constant
y = 248278960456742;
result = (248281107940391 - y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3935: RHS constant
x = 248281107940391;
result = (x - 248278960456742)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3936: both arguments variables
x = 248281107940391;
y = 248283255424038;
result = (x - y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3937: both arguments constants
result = (248281107940391 - 248283255424038)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3938: LHS constant
y = 248283255424038;
result = (248281107940391 - y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3939: RHS constant
x = 248281107940391;
result = (x - 248283255424038)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3940: both arguments variables
x = 248281107940391;
y = 248283255424039;
result = (x - y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3941: both arguments constants
result = (248281107940391 - 248283255424039)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3942: LHS constant
y = 248283255424039;
result = (248281107940391 - y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3943: RHS constant
x = 248281107940391;
result = (x - 248283255424039)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3944: both arguments variables
x = 248281107940391;
y = 248283255424040;
result = (x - y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3945: both arguments constants
result = (248281107940391 - 248283255424040)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3946: LHS constant
y = 248283255424040;
result = (248281107940391 - y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3947: RHS constant
x = 248281107940391;
result = (x - 248283255424040)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3948: both arguments variables
x = 248281107940391;
y = 248283255424041;
result = (x - y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3949: both arguments constants
result = (248281107940391 - 248283255424041)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3950: LHS constant
y = 248283255424041;
result = (248281107940391 - y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3951: RHS constant
x = 248281107940391;
result = (x - 248283255424041)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3952: both arguments variables
x = 248281107940391;
y = 248276812973096;
result = (x - y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3953: both arguments constants
result = (248281107940391 - 248276812973096)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3954: LHS constant
y = 248276812973096;
result = (248281107940391 - y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3955: RHS constant
x = 248281107940391;
result = (x - 248276812973096)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3956: both arguments variables
x = 248281107940391;
y = 248276812973095;
result = (x - y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3957: both arguments constants
result = (248281107940391 - 248276812973095)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3958: LHS constant
y = 248276812973095;
result = (248281107940391 - y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3959: RHS constant
x = 248281107940391;
result = (x - 248276812973095)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3960: both arguments variables
x = 248281107940391;
y = 248285402907686;
result = (x - y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3961: both arguments constants
result = (248281107940391 - 248285402907686)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3962: LHS constant
y = 248285402907686;
result = (248281107940391 - y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3963: RHS constant
x = 248281107940391;
result = (x - 248285402907686)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3964: both arguments variables
x = 248281107940391;
y = 248285402907687;
result = (x - y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 3965: both arguments constants
result = (248281107940391 - 248285402907687)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3966: LHS constant
y = 248285402907687;
result = (248281107940391 - y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3967: RHS constant
x = 248281107940391;
result = (x - 248285402907687)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

}
test0();
test1();
test2();
test3();
test4();
test5();
test6();
test7();
test8();
test9();
test10();
test11();
test12();
test13();
test14();
test15();
test16();
test17();
test18();
test19();
test20();
test21();
test22();
test23();
test24();
test25();
test26();
test27();
test28();
test29();
test30();
test31();
test32();
test33();
test34();
test35();
test36();
test37();
test38();
test39();
WScript.Echo("done");
