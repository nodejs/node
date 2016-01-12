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
y = 1;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1: both arguments constants
result = (0 / 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2: LHS constant
y = 1;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3: RHS constant
x = 0;
result = (x / 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 4: both arguments variables
x = 0;
y = -1;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 5: both arguments constants
result = (0 / -1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 6: LHS constant
y = -1;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 7: RHS constant
x = 0;
result = (x / -1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 8: both arguments variables
x = 0;
y = 2;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 9: both arguments constants
result = (0 / 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 10: LHS constant
y = 2;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 11: RHS constant
x = 0;
result = (x / 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 12: both arguments variables
x = 0;
y = -2;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 13: both arguments constants
result = (0 / -2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 14: LHS constant
y = -2;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 15: RHS constant
x = 0;
result = (x / -2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 16: both arguments variables
x = 0;
y = 3;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 17: both arguments constants
result = (0 / 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 18: LHS constant
y = 3;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 19: RHS constant
x = 0;
result = (x / 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 20: both arguments variables
x = 0;
y = -3;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 21: both arguments constants
result = (0 / -3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 22: LHS constant
y = -3;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 23: RHS constant
x = 0;
result = (x / -3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 24: both arguments variables
x = 0;
y = 4;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 25: both arguments constants
result = (0 / 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 26: LHS constant
y = 4;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 27: RHS constant
x = 0;
result = (x / 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 28: both arguments variables
x = 0;
y = -4;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 29: both arguments constants
result = (0 / -4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 30: LHS constant
y = -4;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 31: RHS constant
x = 0;
result = (x / -4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 32: both arguments variables
x = 0;
y = 8;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 33: both arguments constants
result = (0 / 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 34: LHS constant
y = 8;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 35: RHS constant
x = 0;
result = (x / 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 36: both arguments variables
x = 0;
y = -8;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 37: both arguments constants
result = (0 / -8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 38: LHS constant
y = -8;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 39: RHS constant
x = 0;
result = (x / -8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 40: both arguments variables
x = 0;
y = 1073741822;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 41: both arguments constants
result = (0 / 1073741822)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 42: LHS constant
y = 1073741822;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 43: RHS constant
x = 0;
result = (x / 1073741822)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 44: both arguments variables
x = 0;
y = 1073741823;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 45: both arguments constants
result = (0 / 1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 46: LHS constant
y = 1073741823;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 47: RHS constant
x = 0;
result = (x / 1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 48: both arguments variables
x = 0;
y = 1073741824;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 49: both arguments constants
result = (0 / 1073741824)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 50: LHS constant
y = 1073741824;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 51: RHS constant
x = 0;
result = (x / 1073741824)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 52: both arguments variables
x = 0;
y = 1073741825;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 53: both arguments constants
result = (0 / 1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 54: LHS constant
y = 1073741825;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 55: RHS constant
x = 0;
result = (x / 1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 56: both arguments variables
x = 0;
y = -1073741823;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 57: both arguments constants
result = (0 / -1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 58: LHS constant
y = -1073741823;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 59: RHS constant
x = 0;
result = (x / -1073741823)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 60: both arguments variables
x = 0;
y = (-0x3fffffff-1);
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 61: both arguments constants
result = (0 / (-0x3fffffff-1))
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 62: LHS constant
y = (-0x3fffffff-1);
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 63: RHS constant
x = 0;
result = (x / (-0x3fffffff-1))
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 64: both arguments variables
x = 0;
y = -1073741825;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 65: both arguments constants
result = (0 / -1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 66: LHS constant
y = -1073741825;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 67: RHS constant
x = 0;
result = (x / -1073741825)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 68: both arguments variables
x = 0;
y = -1073741826;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 69: both arguments constants
result = (0 / -1073741826)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 70: LHS constant
y = -1073741826;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 71: RHS constant
x = 0;
result = (x / -1073741826)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 72: both arguments variables
x = 0;
y = 2147483646;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 73: both arguments constants
result = (0 / 2147483646)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 74: LHS constant
y = 2147483646;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 75: RHS constant
x = 0;
result = (x / 2147483646)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 76: both arguments variables
x = 0;
y = 2147483647;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 77: both arguments constants
result = (0 / 2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 78: LHS constant
y = 2147483647;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 79: RHS constant
x = 0;
result = (x / 2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 80: both arguments variables
x = 0;
y = 2147483648;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 81: both arguments constants
result = (0 / 2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 82: LHS constant
y = 2147483648;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 83: RHS constant
x = 0;
result = (x / 2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 84: both arguments variables
x = 0;
y = 2147483649;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 85: both arguments constants
result = (0 / 2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 86: LHS constant
y = 2147483649;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 87: RHS constant
x = 0;
result = (x / 2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 88: both arguments variables
x = 0;
y = -2147483647;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 89: both arguments constants
result = (0 / -2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 90: LHS constant
y = -2147483647;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 91: RHS constant
x = 0;
result = (x / -2147483647)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 92: both arguments variables
x = 0;
y = -2147483648;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 93: both arguments constants
result = (0 / -2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 94: LHS constant
y = -2147483648;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 95: RHS constant
x = 0;
result = (x / -2147483648)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 96: both arguments variables
x = 0;
y = -2147483649;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 97: both arguments constants
result = (0 / -2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 98: LHS constant
y = -2147483649;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 99: RHS constant
x = 0;
result = (x / -2147483649)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test1() {
var x;
var y;
var result;
var check;
// Test 100: both arguments variables
x = 0;
y = -2147483650;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 101: both arguments constants
result = (0 / -2147483650)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 102: LHS constant
y = -2147483650;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 103: RHS constant
x = 0;
result = (x / -2147483650)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 104: both arguments variables
x = 0;
y = 4294967295;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 105: both arguments constants
result = (0 / 4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 106: LHS constant
y = 4294967295;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 107: RHS constant
x = 0;
result = (x / 4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 108: both arguments variables
x = 0;
y = 4294967296;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 109: both arguments constants
result = (0 / 4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 110: LHS constant
y = 4294967296;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 111: RHS constant
x = 0;
result = (x / 4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 112: both arguments variables
x = 0;
y = -4294967295;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 113: both arguments constants
result = (0 / -4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 114: LHS constant
y = -4294967295;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 115: RHS constant
x = 0;
result = (x / -4294967295)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 116: both arguments variables
x = 0;
y = -4294967296;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 117: both arguments constants
result = (0 / -4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 118: LHS constant
y = -4294967296;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 119: RHS constant
x = 0;
result = (x / -4294967296)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 120: both arguments variables
x = 1;
y = 1;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 121: both arguments constants
result = (1 / 1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 122: LHS constant
y = 1;
result = (1 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 123: RHS constant
x = 1;
result = (x / 1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 124: both arguments variables
x = 1;
y = -1;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 125: both arguments constants
result = (1 / -1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 126: LHS constant
y = -1;
result = (1 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 127: RHS constant
x = 1;
result = (x / -1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 128: both arguments variables
x = -1;
y = 1;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 129: both arguments constants
result = (-1 / 1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 130: LHS constant
y = 1;
result = (-1 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 131: RHS constant
x = -1;
result = (x / 1)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 132: both arguments variables
x = -1;
y = -1;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 133: both arguments constants
result = (-1 / -1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 134: LHS constant
y = -1;
result = (-1 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 135: RHS constant
x = -1;
result = (x / -1)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 136: both arguments variables
x = 2;
y = 1;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 137: both arguments constants
result = (2 / 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 138: LHS constant
y = 1;
result = (2 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 139: RHS constant
x = 2;
result = (x / 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 140: both arguments variables
x = 2;
y = -1;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 141: both arguments constants
result = (2 / -1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 142: LHS constant
y = -1;
result = (2 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 143: RHS constant
x = 2;
result = (x / -1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 144: both arguments variables
x = 2;
y = 2;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 145: both arguments constants
result = (2 / 2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 146: LHS constant
y = 2;
result = (2 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 147: RHS constant
x = 2;
result = (x / 2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 148: both arguments variables
x = 2;
y = -2;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 149: both arguments constants
result = (2 / -2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 150: LHS constant
y = -2;
result = (2 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 151: RHS constant
x = 2;
result = (x / -2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 152: both arguments variables
x = -2;
y = 1;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 153: both arguments constants
result = (-2 / 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 154: LHS constant
y = 1;
result = (-2 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 155: RHS constant
x = -2;
result = (x / 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 156: both arguments variables
x = -2;
y = -1;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 157: both arguments constants
result = (-2 / -1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 158: LHS constant
y = -1;
result = (-2 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 159: RHS constant
x = -2;
result = (x / -1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 160: both arguments variables
x = -2;
y = 2;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 161: both arguments constants
result = (-2 / 2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 162: LHS constant
y = 2;
result = (-2 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 163: RHS constant
x = -2;
result = (x / 2)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 164: both arguments variables
x = -2;
y = -2;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 165: both arguments constants
result = (-2 / -2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 166: LHS constant
y = -2;
result = (-2 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 167: RHS constant
x = -2;
result = (x / -2)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 168: both arguments variables
x = 3;
y = 1;
result = (x / y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 169: both arguments constants
result = (3 / 1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 170: LHS constant
y = 1;
result = (3 / y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 171: RHS constant
x = 3;
result = (x / 1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 172: both arguments variables
x = 3;
y = -1;
result = (x / y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 173: both arguments constants
result = (3 / -1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 174: LHS constant
y = -1;
result = (3 / y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 175: RHS constant
x = 3;
result = (x / -1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 176: both arguments variables
x = 3;
y = 3;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 177: both arguments constants
result = (3 / 3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 178: LHS constant
y = 3;
result = (3 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 179: RHS constant
x = 3;
result = (x / 3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 180: both arguments variables
x = 3;
y = -3;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 181: both arguments constants
result = (3 / -3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 182: LHS constant
y = -3;
result = (3 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 183: RHS constant
x = 3;
result = (x / -3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 184: both arguments variables
x = -3;
y = 1;
result = (x / y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 185: both arguments constants
result = (-3 / 1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 186: LHS constant
y = 1;
result = (-3 / y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 187: RHS constant
x = -3;
result = (x / 1)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 188: both arguments variables
x = -3;
y = -1;
result = (x / y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 189: both arguments constants
result = (-3 / -1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 190: LHS constant
y = -1;
result = (-3 / y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 191: RHS constant
x = -3;
result = (x / -1)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 192: both arguments variables
x = -3;
y = 3;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 193: both arguments constants
result = (-3 / 3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 194: LHS constant
y = 3;
result = (-3 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 195: RHS constant
x = -3;
result = (x / 3)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 196: both arguments variables
x = -3;
y = -3;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 197: both arguments constants
result = (-3 / -3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 198: LHS constant
y = -3;
result = (-3 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 199: RHS constant
x = -3;
result = (x / -3)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test2() {
var x;
var y;
var result;
var check;
// Test 200: both arguments variables
x = 4;
y = 1;
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 201: both arguments constants
result = (4 / 1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 202: LHS constant
y = 1;
result = (4 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 203: RHS constant
x = 4;
result = (x / 1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 204: both arguments variables
x = 4;
y = -1;
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 205: both arguments constants
result = (4 / -1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 206: LHS constant
y = -1;
result = (4 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 207: RHS constant
x = 4;
result = (x / -1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 208: both arguments variables
x = 4;
y = 2;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 209: both arguments constants
result = (4 / 2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 210: LHS constant
y = 2;
result = (4 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 211: RHS constant
x = 4;
result = (x / 2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 212: both arguments variables
x = 4;
y = -2;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 213: both arguments constants
result = (4 / -2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 214: LHS constant
y = -2;
result = (4 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 215: RHS constant
x = 4;
result = (x / -2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 216: both arguments variables
x = 4;
y = 4;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 217: both arguments constants
result = (4 / 4)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 218: LHS constant
y = 4;
result = (4 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 219: RHS constant
x = 4;
result = (x / 4)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 220: both arguments variables
x = 4;
y = -4;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 221: both arguments constants
result = (4 / -4)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 222: LHS constant
y = -4;
result = (4 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 223: RHS constant
x = 4;
result = (x / -4)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 224: both arguments variables
x = -4;
y = 1;
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 225: both arguments constants
result = (-4 / 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 226: LHS constant
y = 1;
result = (-4 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 227: RHS constant
x = -4;
result = (x / 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 228: both arguments variables
x = -4;
y = -1;
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 229: both arguments constants
result = (-4 / -1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 230: LHS constant
y = -1;
result = (-4 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 231: RHS constant
x = -4;
result = (x / -1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 232: both arguments variables
x = -4;
y = 2;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 233: both arguments constants
result = (-4 / 2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 234: LHS constant
y = 2;
result = (-4 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 235: RHS constant
x = -4;
result = (x / 2)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 236: both arguments variables
x = -4;
y = -2;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 237: both arguments constants
result = (-4 / -2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 238: LHS constant
y = -2;
result = (-4 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 239: RHS constant
x = -4;
result = (x / -2)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 240: both arguments variables
x = -4;
y = 4;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 241: both arguments constants
result = (-4 / 4)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 242: LHS constant
y = 4;
result = (-4 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 243: RHS constant
x = -4;
result = (x / 4)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 244: both arguments variables
x = -4;
y = -4;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 245: both arguments constants
result = (-4 / -4)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 246: LHS constant
y = -4;
result = (-4 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 247: RHS constant
x = -4;
result = (x / -4)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 248: both arguments variables
x = 8;
y = 1;
result = (x / y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 249: both arguments constants
result = (8 / 1)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 250: LHS constant
y = 1;
result = (8 / y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 251: RHS constant
x = 8;
result = (x / 1)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 252: both arguments variables
x = 8;
y = -1;
result = (x / y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 253: both arguments constants
result = (8 / -1)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 254: LHS constant
y = -1;
result = (8 / y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 255: RHS constant
x = 8;
result = (x / -1)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 256: both arguments variables
x = 8;
y = 2;
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 257: both arguments constants
result = (8 / 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 258: LHS constant
y = 2;
result = (8 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 259: RHS constant
x = 8;
result = (x / 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 260: both arguments variables
x = 8;
y = -2;
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 261: both arguments constants
result = (8 / -2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 262: LHS constant
y = -2;
result = (8 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 263: RHS constant
x = 8;
result = (x / -2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 264: both arguments variables
x = 8;
y = 4;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 265: both arguments constants
result = (8 / 4)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 266: LHS constant
y = 4;
result = (8 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 267: RHS constant
x = 8;
result = (x / 4)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 268: both arguments variables
x = 8;
y = -4;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 269: both arguments constants
result = (8 / -4)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 270: LHS constant
y = -4;
result = (8 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 271: RHS constant
x = 8;
result = (x / -4)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 272: both arguments variables
x = 8;
y = 8;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 273: both arguments constants
result = (8 / 8)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 274: LHS constant
y = 8;
result = (8 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 275: RHS constant
x = 8;
result = (x / 8)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 276: both arguments variables
x = 8;
y = -8;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 277: both arguments constants
result = (8 / -8)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 278: LHS constant
y = -8;
result = (8 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 279: RHS constant
x = 8;
result = (x / -8)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 280: both arguments variables
x = -8;
y = 1;
result = (x / y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 281: both arguments constants
result = (-8 / 1)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 282: LHS constant
y = 1;
result = (-8 / y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 283: RHS constant
x = -8;
result = (x / 1)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 284: both arguments variables
x = -8;
y = -1;
result = (x / y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 285: both arguments constants
result = (-8 / -1)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 286: LHS constant
y = -1;
result = (-8 / y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 287: RHS constant
x = -8;
result = (x / -1)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 288: both arguments variables
x = -8;
y = 2;
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 289: both arguments constants
result = (-8 / 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 290: LHS constant
y = 2;
result = (-8 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 291: RHS constant
x = -8;
result = (x / 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 292: both arguments variables
x = -8;
y = -2;
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 293: both arguments constants
result = (-8 / -2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 294: LHS constant
y = -2;
result = (-8 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 295: RHS constant
x = -8;
result = (x / -2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 296: both arguments variables
x = -8;
y = 4;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 297: both arguments constants
result = (-8 / 4)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 298: LHS constant
y = 4;
result = (-8 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 299: RHS constant
x = -8;
result = (x / 4)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test3() {
var x;
var y;
var result;
var check;
// Test 300: both arguments variables
x = -8;
y = -4;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 301: both arguments constants
result = (-8 / -4)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 302: LHS constant
y = -4;
result = (-8 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 303: RHS constant
x = -8;
result = (x / -4)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 304: both arguments variables
x = -8;
y = 8;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 305: both arguments constants
result = (-8 / 8)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 306: LHS constant
y = 8;
result = (-8 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 307: RHS constant
x = -8;
result = (x / 8)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 308: both arguments variables
x = -8;
y = -8;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 309: both arguments constants
result = (-8 / -8)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 310: LHS constant
y = -8;
result = (-8 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 311: RHS constant
x = -8;
result = (x / -8)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 312: both arguments variables
x = 1073741822;
y = 1;
result = (x / y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 313: both arguments constants
result = (1073741822 / 1)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 314: LHS constant
y = 1;
result = (1073741822 / y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 315: RHS constant
x = 1073741822;
result = (x / 1)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 316: both arguments variables
x = 1073741822;
y = -1;
result = (x / y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 317: both arguments constants
result = (1073741822 / -1)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 318: LHS constant
y = -1;
result = (1073741822 / y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 319: RHS constant
x = 1073741822;
result = (x / -1)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 320: both arguments variables
x = 1073741822;
y = 2;
result = (x / y);
check = 536870911;
if(result != check) { fail(test, check, result); } ++test; 

// Test 321: both arguments constants
result = (1073741822 / 2)
check = 536870911
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 322: LHS constant
y = 2;
result = (1073741822 / y)
check = 536870911
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 323: RHS constant
x = 1073741822;
result = (x / 2)
check = 536870911
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 324: both arguments variables
x = 1073741822;
y = -2;
result = (x / y);
check = -536870911;
if(result != check) { fail(test, check, result); } ++test; 

// Test 325: both arguments constants
result = (1073741822 / -2)
check = -536870911
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 326: LHS constant
y = -2;
result = (1073741822 / y)
check = -536870911
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 327: RHS constant
x = 1073741822;
result = (x / -2)
check = -536870911
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 328: both arguments variables
x = 1073741822;
y = 1073741822;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 329: both arguments constants
result = (1073741822 / 1073741822)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 330: LHS constant
y = 1073741822;
result = (1073741822 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 331: RHS constant
x = 1073741822;
result = (x / 1073741822)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 332: both arguments variables
x = 1073741823;
y = 1;
result = (x / y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 333: both arguments constants
result = (1073741823 / 1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 334: LHS constant
y = 1;
result = (1073741823 / y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 335: RHS constant
x = 1073741823;
result = (x / 1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 336: both arguments variables
x = 1073741823;
y = -1;
result = (x / y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 337: both arguments constants
result = (1073741823 / -1)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 338: LHS constant
y = -1;
result = (1073741823 / y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 339: RHS constant
x = 1073741823;
result = (x / -1)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 340: both arguments variables
x = 1073741823;
y = 3;
result = (x / y);
check = 357913941;
if(result != check) { fail(test, check, result); } ++test; 

// Test 341: both arguments constants
result = (1073741823 / 3)
check = 357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 342: LHS constant
y = 3;
result = (1073741823 / y)
check = 357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 343: RHS constant
x = 1073741823;
result = (x / 3)
check = 357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 344: both arguments variables
x = 1073741823;
y = -3;
result = (x / y);
check = -357913941;
if(result != check) { fail(test, check, result); } ++test; 

// Test 345: both arguments constants
result = (1073741823 / -3)
check = -357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 346: LHS constant
y = -3;
result = (1073741823 / y)
check = -357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 347: RHS constant
x = 1073741823;
result = (x / -3)
check = -357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 348: both arguments variables
x = 1073741823;
y = 1073741823;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 349: both arguments constants
result = (1073741823 / 1073741823)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 350: LHS constant
y = 1073741823;
result = (1073741823 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 351: RHS constant
x = 1073741823;
result = (x / 1073741823)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 352: both arguments variables
x = 1073741823;
y = -1073741823;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 353: both arguments constants
result = (1073741823 / -1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 354: LHS constant
y = -1073741823;
result = (1073741823 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 355: RHS constant
x = 1073741823;
result = (x / -1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 356: both arguments variables
x = 1073741824;
y = 1;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 357: both arguments constants
result = (1073741824 / 1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 358: LHS constant
y = 1;
result = (1073741824 / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 359: RHS constant
x = 1073741824;
result = (x / 1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 360: both arguments variables
x = 1073741824;
y = -1;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 361: both arguments constants
result = (1073741824 / -1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 362: LHS constant
y = -1;
result = (1073741824 / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 363: RHS constant
x = 1073741824;
result = (x / -1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 364: both arguments variables
x = 1073741824;
y = 2;
result = (x / y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 365: both arguments constants
result = (1073741824 / 2)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 366: LHS constant
y = 2;
result = (1073741824 / y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 367: RHS constant
x = 1073741824;
result = (x / 2)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 368: both arguments variables
x = 1073741824;
y = -2;
result = (x / y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 369: both arguments constants
result = (1073741824 / -2)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 370: LHS constant
y = -2;
result = (1073741824 / y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 371: RHS constant
x = 1073741824;
result = (x / -2)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 372: both arguments variables
x = 1073741824;
y = 4;
result = (x / y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 373: both arguments constants
result = (1073741824 / 4)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 374: LHS constant
y = 4;
result = (1073741824 / y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 375: RHS constant
x = 1073741824;
result = (x / 4)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 376: both arguments variables
x = 1073741824;
y = -4;
result = (x / y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 377: both arguments constants
result = (1073741824 / -4)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 378: LHS constant
y = -4;
result = (1073741824 / y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 379: RHS constant
x = 1073741824;
result = (x / -4)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 380: both arguments variables
x = 1073741824;
y = 8;
result = (x / y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 381: both arguments constants
result = (1073741824 / 8)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 382: LHS constant
y = 8;
result = (1073741824 / y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 383: RHS constant
x = 1073741824;
result = (x / 8)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 384: both arguments variables
x = 1073741824;
y = -8;
result = (x / y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 385: both arguments constants
result = (1073741824 / -8)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 386: LHS constant
y = -8;
result = (1073741824 / y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 387: RHS constant
x = 1073741824;
result = (x / -8)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 388: both arguments variables
x = 1073741824;
y = 1073741824;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 389: both arguments constants
result = (1073741824 / 1073741824)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 390: LHS constant
y = 1073741824;
result = (1073741824 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 391: RHS constant
x = 1073741824;
result = (x / 1073741824)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 392: both arguments variables
x = 1073741824;
y = (-0x3fffffff-1);
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 393: both arguments constants
result = (1073741824 / (-0x3fffffff-1))
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 394: LHS constant
y = (-0x3fffffff-1);
result = (1073741824 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 395: RHS constant
x = 1073741824;
result = (x / (-0x3fffffff-1))
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 396: both arguments variables
x = 1073741825;
y = 1;
result = (x / y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 397: both arguments constants
result = (1073741825 / 1)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 398: LHS constant
y = 1;
result = (1073741825 / y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 399: RHS constant
x = 1073741825;
result = (x / 1)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test4() {
var x;
var y;
var result;
var check;
// Test 400: both arguments variables
x = 1073741825;
y = -1;
result = (x / y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 401: both arguments constants
result = (1073741825 / -1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 402: LHS constant
y = -1;
result = (1073741825 / y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 403: RHS constant
x = 1073741825;
result = (x / -1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 404: both arguments variables
x = 1073741825;
y = 1073741825;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 405: both arguments constants
result = (1073741825 / 1073741825)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 406: LHS constant
y = 1073741825;
result = (1073741825 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 407: RHS constant
x = 1073741825;
result = (x / 1073741825)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 408: both arguments variables
x = 1073741825;
y = -1073741825;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 409: both arguments constants
result = (1073741825 / -1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 410: LHS constant
y = -1073741825;
result = (1073741825 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 411: RHS constant
x = 1073741825;
result = (x / -1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 412: both arguments variables
x = -1073741823;
y = 1;
result = (x / y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 413: both arguments constants
result = (-1073741823 / 1)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 414: LHS constant
y = 1;
result = (-1073741823 / y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 415: RHS constant
x = -1073741823;
result = (x / 1)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 416: both arguments variables
x = -1073741823;
y = -1;
result = (x / y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 417: both arguments constants
result = (-1073741823 / -1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 418: LHS constant
y = -1;
result = (-1073741823 / y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 419: RHS constant
x = -1073741823;
result = (x / -1)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 420: both arguments variables
x = -1073741823;
y = 3;
result = (x / y);
check = -357913941;
if(result != check) { fail(test, check, result); } ++test; 

// Test 421: both arguments constants
result = (-1073741823 / 3)
check = -357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 422: LHS constant
y = 3;
result = (-1073741823 / y)
check = -357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 423: RHS constant
x = -1073741823;
result = (x / 3)
check = -357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 424: both arguments variables
x = -1073741823;
y = -3;
result = (x / y);
check = 357913941;
if(result != check) { fail(test, check, result); } ++test; 

// Test 425: both arguments constants
result = (-1073741823 / -3)
check = 357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 426: LHS constant
y = -3;
result = (-1073741823 / y)
check = 357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 427: RHS constant
x = -1073741823;
result = (x / -3)
check = 357913941
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 428: both arguments variables
x = -1073741823;
y = 1073741823;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 429: both arguments constants
result = (-1073741823 / 1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 430: LHS constant
y = 1073741823;
result = (-1073741823 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 431: RHS constant
x = -1073741823;
result = (x / 1073741823)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 432: both arguments variables
x = -1073741823;
y = -1073741823;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 433: both arguments constants
result = (-1073741823 / -1073741823)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 434: LHS constant
y = -1073741823;
result = (-1073741823 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 435: RHS constant
x = -1073741823;
result = (x / -1073741823)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 436: both arguments variables
x = (-0x3fffffff-1);
y = 1;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 437: both arguments constants
result = ((-0x3fffffff-1) / 1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 438: LHS constant
y = 1;
result = ((-0x3fffffff-1) / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 439: RHS constant
x = (-0x3fffffff-1);
result = (x / 1)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 440: both arguments variables
x = (-0x3fffffff-1);
y = -1;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 441: both arguments constants
result = ((-0x3fffffff-1) / -1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 442: LHS constant
y = -1;
result = ((-0x3fffffff-1) / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 443: RHS constant
x = (-0x3fffffff-1);
result = (x / -1)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 444: both arguments variables
x = (-0x3fffffff-1);
y = 2;
result = (x / y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 445: both arguments constants
result = ((-0x3fffffff-1) / 2)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 446: LHS constant
y = 2;
result = ((-0x3fffffff-1) / y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 447: RHS constant
x = (-0x3fffffff-1);
result = (x / 2)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 448: both arguments variables
x = (-0x3fffffff-1);
y = -2;
result = (x / y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 449: both arguments constants
result = ((-0x3fffffff-1) / -2)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 450: LHS constant
y = -2;
result = ((-0x3fffffff-1) / y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 451: RHS constant
x = (-0x3fffffff-1);
result = (x / -2)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 452: both arguments variables
x = (-0x3fffffff-1);
y = 4;
result = (x / y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 453: both arguments constants
result = ((-0x3fffffff-1) / 4)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 454: LHS constant
y = 4;
result = ((-0x3fffffff-1) / y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 455: RHS constant
x = (-0x3fffffff-1);
result = (x / 4)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 456: both arguments variables
x = (-0x3fffffff-1);
y = -4;
result = (x / y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 457: both arguments constants
result = ((-0x3fffffff-1) / -4)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 458: LHS constant
y = -4;
result = ((-0x3fffffff-1) / y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 459: RHS constant
x = (-0x3fffffff-1);
result = (x / -4)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 460: both arguments variables
x = (-0x3fffffff-1);
y = 8;
result = (x / y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 461: both arguments constants
result = ((-0x3fffffff-1) / 8)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 462: LHS constant
y = 8;
result = ((-0x3fffffff-1) / y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 463: RHS constant
x = (-0x3fffffff-1);
result = (x / 8)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 464: both arguments variables
x = (-0x3fffffff-1);
y = -8;
result = (x / y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 465: both arguments constants
result = ((-0x3fffffff-1) / -8)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 466: LHS constant
y = -8;
result = ((-0x3fffffff-1) / y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 467: RHS constant
x = (-0x3fffffff-1);
result = (x / -8)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 468: both arguments variables
x = (-0x3fffffff-1);
y = 1073741824;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 469: both arguments constants
result = ((-0x3fffffff-1) / 1073741824)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 470: LHS constant
y = 1073741824;
result = ((-0x3fffffff-1) / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 471: RHS constant
x = (-0x3fffffff-1);
result = (x / 1073741824)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 472: both arguments variables
x = (-0x3fffffff-1);
y = (-0x3fffffff-1);
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 473: both arguments constants
result = ((-0x3fffffff-1) / (-0x3fffffff-1))
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 474: LHS constant
y = (-0x3fffffff-1);
result = ((-0x3fffffff-1) / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 475: RHS constant
x = (-0x3fffffff-1);
result = (x / (-0x3fffffff-1))
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 476: both arguments variables
x = -1073741825;
y = 1;
result = (x / y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 477: both arguments constants
result = (-1073741825 / 1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 478: LHS constant
y = 1;
result = (-1073741825 / y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 479: RHS constant
x = -1073741825;
result = (x / 1)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 480: both arguments variables
x = -1073741825;
y = -1;
result = (x / y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 481: both arguments constants
result = (-1073741825 / -1)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 482: LHS constant
y = -1;
result = (-1073741825 / y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 483: RHS constant
x = -1073741825;
result = (x / -1)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 484: both arguments variables
x = -1073741825;
y = 1073741825;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 485: both arguments constants
result = (-1073741825 / 1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 486: LHS constant
y = 1073741825;
result = (-1073741825 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 487: RHS constant
x = -1073741825;
result = (x / 1073741825)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 488: both arguments variables
x = -1073741825;
y = -1073741825;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 489: both arguments constants
result = (-1073741825 / -1073741825)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 490: LHS constant
y = -1073741825;
result = (-1073741825 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 491: RHS constant
x = -1073741825;
result = (x / -1073741825)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 492: both arguments variables
x = -1073741826;
y = 1;
result = (x / y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 493: both arguments constants
result = (-1073741826 / 1)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 494: LHS constant
y = 1;
result = (-1073741826 / y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 495: RHS constant
x = -1073741826;
result = (x / 1)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 496: both arguments variables
x = -1073741826;
y = -1;
result = (x / y);
check = 1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 497: both arguments constants
result = (-1073741826 / -1)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 498: LHS constant
y = -1;
result = (-1073741826 / y)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 499: RHS constant
x = -1073741826;
result = (x / -1)
check = 1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test5() {
var x;
var y;
var result;
var check;
// Test 500: both arguments variables
x = -1073741826;
y = 2;
result = (x / y);
check = -536870913;
if(result != check) { fail(test, check, result); } ++test; 

// Test 501: both arguments constants
result = (-1073741826 / 2)
check = -536870913
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 502: LHS constant
y = 2;
result = (-1073741826 / y)
check = -536870913
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 503: RHS constant
x = -1073741826;
result = (x / 2)
check = -536870913
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 504: both arguments variables
x = -1073741826;
y = -2;
result = (x / y);
check = 536870913;
if(result != check) { fail(test, check, result); } ++test; 

// Test 505: both arguments constants
result = (-1073741826 / -2)
check = 536870913
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 506: LHS constant
y = -2;
result = (-1073741826 / y)
check = 536870913
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 507: RHS constant
x = -1073741826;
result = (x / -2)
check = 536870913
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 508: both arguments variables
x = -1073741826;
y = 3;
result = (x / y);
check = -357913942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 509: both arguments constants
result = (-1073741826 / 3)
check = -357913942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 510: LHS constant
y = 3;
result = (-1073741826 / y)
check = -357913942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 511: RHS constant
x = -1073741826;
result = (x / 3)
check = -357913942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 512: both arguments variables
x = -1073741826;
y = -3;
result = (x / y);
check = 357913942;
if(result != check) { fail(test, check, result); } ++test; 

// Test 513: both arguments constants
result = (-1073741826 / -3)
check = 357913942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 514: LHS constant
y = -3;
result = (-1073741826 / y)
check = 357913942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 515: RHS constant
x = -1073741826;
result = (x / -3)
check = 357913942
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 516: both arguments variables
x = -1073741826;
y = -1073741826;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 517: both arguments constants
result = (-1073741826 / -1073741826)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 518: LHS constant
y = -1073741826;
result = (-1073741826 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 519: RHS constant
x = -1073741826;
result = (x / -1073741826)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 520: both arguments variables
x = 2147483646;
y = 1;
result = (x / y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 521: both arguments constants
result = (2147483646 / 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 522: LHS constant
y = 1;
result = (2147483646 / y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 523: RHS constant
x = 2147483646;
result = (x / 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 524: both arguments variables
x = 2147483646;
y = -1;
result = (x / y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 525: both arguments constants
result = (2147483646 / -1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 526: LHS constant
y = -1;
result = (2147483646 / y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 527: RHS constant
x = 2147483646;
result = (x / -1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 528: both arguments variables
x = 2147483646;
y = 2;
result = (x / y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 529: both arguments constants
result = (2147483646 / 2)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 530: LHS constant
y = 2;
result = (2147483646 / y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 531: RHS constant
x = 2147483646;
result = (x / 2)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 532: both arguments variables
x = 2147483646;
y = -2;
result = (x / y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 533: both arguments constants
result = (2147483646 / -2)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 534: LHS constant
y = -2;
result = (2147483646 / y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 535: RHS constant
x = 2147483646;
result = (x / -2)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 536: both arguments variables
x = 2147483646;
y = 3;
result = (x / y);
check = 715827882;
if(result != check) { fail(test, check, result); } ++test; 

// Test 537: both arguments constants
result = (2147483646 / 3)
check = 715827882
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 538: LHS constant
y = 3;
result = (2147483646 / y)
check = 715827882
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 539: RHS constant
x = 2147483646;
result = (x / 3)
check = 715827882
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 540: both arguments variables
x = 2147483646;
y = -3;
result = (x / y);
check = -715827882;
if(result != check) { fail(test, check, result); } ++test; 

// Test 541: both arguments constants
result = (2147483646 / -3)
check = -715827882
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 542: LHS constant
y = -3;
result = (2147483646 / y)
check = -715827882
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 543: RHS constant
x = 2147483646;
result = (x / -3)
check = -715827882
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 544: both arguments variables
x = 2147483646;
y = 1073741823;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 545: both arguments constants
result = (2147483646 / 1073741823)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 546: LHS constant
y = 1073741823;
result = (2147483646 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 547: RHS constant
x = 2147483646;
result = (x / 1073741823)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 548: both arguments variables
x = 2147483646;
y = -1073741823;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 549: both arguments constants
result = (2147483646 / -1073741823)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 550: LHS constant
y = -1073741823;
result = (2147483646 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 551: RHS constant
x = 2147483646;
result = (x / -1073741823)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 552: both arguments variables
x = 2147483646;
y = 2147483646;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 553: both arguments constants
result = (2147483646 / 2147483646)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 554: LHS constant
y = 2147483646;
result = (2147483646 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 555: RHS constant
x = 2147483646;
result = (x / 2147483646)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 556: both arguments variables
x = 2147483647;
y = 1;
result = (x / y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 557: both arguments constants
result = (2147483647 / 1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 558: LHS constant
y = 1;
result = (2147483647 / y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 559: RHS constant
x = 2147483647;
result = (x / 1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 560: both arguments variables
x = 2147483647;
y = -1;
result = (x / y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 561: both arguments constants
result = (2147483647 / -1)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 562: LHS constant
y = -1;
result = (2147483647 / y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 563: RHS constant
x = 2147483647;
result = (x / -1)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 564: both arguments variables
x = 2147483647;
y = 2147483647;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 565: both arguments constants
result = (2147483647 / 2147483647)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 566: LHS constant
y = 2147483647;
result = (2147483647 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 567: RHS constant
x = 2147483647;
result = (x / 2147483647)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 568: both arguments variables
x = 2147483647;
y = -2147483647;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 569: both arguments constants
result = (2147483647 / -2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 570: LHS constant
y = -2147483647;
result = (2147483647 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 571: RHS constant
x = 2147483647;
result = (x / -2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 572: both arguments variables
x = 2147483648;
y = 1;
result = (x / y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 573: both arguments constants
result = (2147483648 / 1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 574: LHS constant
y = 1;
result = (2147483648 / y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 575: RHS constant
x = 2147483648;
result = (x / 1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 576: both arguments variables
x = 2147483648;
y = -1;
result = (x / y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 577: both arguments constants
result = (2147483648 / -1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 578: LHS constant
y = -1;
result = (2147483648 / y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 579: RHS constant
x = 2147483648;
result = (x / -1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 580: both arguments variables
x = 2147483648;
y = 2;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 581: both arguments constants
result = (2147483648 / 2)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 582: LHS constant
y = 2;
result = (2147483648 / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 583: RHS constant
x = 2147483648;
result = (x / 2)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 584: both arguments variables
x = 2147483648;
y = -2;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 585: both arguments constants
result = (2147483648 / -2)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 586: LHS constant
y = -2;
result = (2147483648 / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 587: RHS constant
x = 2147483648;
result = (x / -2)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 588: both arguments variables
x = 2147483648;
y = 4;
result = (x / y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 589: both arguments constants
result = (2147483648 / 4)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 590: LHS constant
y = 4;
result = (2147483648 / y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 591: RHS constant
x = 2147483648;
result = (x / 4)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 592: both arguments variables
x = 2147483648;
y = -4;
result = (x / y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 593: both arguments constants
result = (2147483648 / -4)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 594: LHS constant
y = -4;
result = (2147483648 / y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 595: RHS constant
x = 2147483648;
result = (x / -4)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 596: both arguments variables
x = 2147483648;
y = 8;
result = (x / y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 597: both arguments constants
result = (2147483648 / 8)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 598: LHS constant
y = 8;
result = (2147483648 / y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 599: RHS constant
x = 2147483648;
result = (x / 8)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test6() {
var x;
var y;
var result;
var check;
// Test 600: both arguments variables
x = 2147483648;
y = -8;
result = (x / y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 601: both arguments constants
result = (2147483648 / -8)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 602: LHS constant
y = -8;
result = (2147483648 / y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 603: RHS constant
x = 2147483648;
result = (x / -8)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 604: both arguments variables
x = 2147483648;
y = 1073741824;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 605: both arguments constants
result = (2147483648 / 1073741824)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 606: LHS constant
y = 1073741824;
result = (2147483648 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 607: RHS constant
x = 2147483648;
result = (x / 1073741824)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 608: both arguments variables
x = 2147483648;
y = (-0x3fffffff-1);
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 609: both arguments constants
result = (2147483648 / (-0x3fffffff-1))
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 610: LHS constant
y = (-0x3fffffff-1);
result = (2147483648 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 611: RHS constant
x = 2147483648;
result = (x / (-0x3fffffff-1))
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 612: both arguments variables
x = 2147483648;
y = 2147483648;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 613: both arguments constants
result = (2147483648 / 2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 614: LHS constant
y = 2147483648;
result = (2147483648 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 615: RHS constant
x = 2147483648;
result = (x / 2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 616: both arguments variables
x = 2147483648;
y = -2147483648;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 617: both arguments constants
result = (2147483648 / -2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 618: LHS constant
y = -2147483648;
result = (2147483648 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 619: RHS constant
x = 2147483648;
result = (x / -2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 620: both arguments variables
x = 2147483649;
y = 1;
result = (x / y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 621: both arguments constants
result = (2147483649 / 1)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 622: LHS constant
y = 1;
result = (2147483649 / y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 623: RHS constant
x = 2147483649;
result = (x / 1)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 624: both arguments variables
x = 2147483649;
y = -1;
result = (x / y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 625: both arguments constants
result = (2147483649 / -1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 626: LHS constant
y = -1;
result = (2147483649 / y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 627: RHS constant
x = 2147483649;
result = (x / -1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 628: both arguments variables
x = 2147483649;
y = 3;
result = (x / y);
check = 715827883;
if(result != check) { fail(test, check, result); } ++test; 

// Test 629: both arguments constants
result = (2147483649 / 3)
check = 715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 630: LHS constant
y = 3;
result = (2147483649 / y)
check = 715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 631: RHS constant
x = 2147483649;
result = (x / 3)
check = 715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 632: both arguments variables
x = 2147483649;
y = -3;
result = (x / y);
check = -715827883;
if(result != check) { fail(test, check, result); } ++test; 

// Test 633: both arguments constants
result = (2147483649 / -3)
check = -715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 634: LHS constant
y = -3;
result = (2147483649 / y)
check = -715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 635: RHS constant
x = 2147483649;
result = (x / -3)
check = -715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 636: both arguments variables
x = 2147483649;
y = 2147483649;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 637: both arguments constants
result = (2147483649 / 2147483649)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 638: LHS constant
y = 2147483649;
result = (2147483649 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 639: RHS constant
x = 2147483649;
result = (x / 2147483649)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 640: both arguments variables
x = 2147483649;
y = -2147483649;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 641: both arguments constants
result = (2147483649 / -2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 642: LHS constant
y = -2147483649;
result = (2147483649 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 643: RHS constant
x = 2147483649;
result = (x / -2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 644: both arguments variables
x = -2147483647;
y = 1;
result = (x / y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 645: both arguments constants
result = (-2147483647 / 1)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 646: LHS constant
y = 1;
result = (-2147483647 / y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 647: RHS constant
x = -2147483647;
result = (x / 1)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 648: both arguments variables
x = -2147483647;
y = -1;
result = (x / y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 649: both arguments constants
result = (-2147483647 / -1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 650: LHS constant
y = -1;
result = (-2147483647 / y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 651: RHS constant
x = -2147483647;
result = (x / -1)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 652: both arguments variables
x = -2147483647;
y = 2147483647;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 653: both arguments constants
result = (-2147483647 / 2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 654: LHS constant
y = 2147483647;
result = (-2147483647 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 655: RHS constant
x = -2147483647;
result = (x / 2147483647)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 656: both arguments variables
x = -2147483647;
y = -2147483647;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 657: both arguments constants
result = (-2147483647 / -2147483647)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 658: LHS constant
y = -2147483647;
result = (-2147483647 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 659: RHS constant
x = -2147483647;
result = (x / -2147483647)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 660: both arguments variables
x = -2147483648;
y = 1;
result = (x / y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 661: both arguments constants
result = (-2147483648 / 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 662: LHS constant
y = 1;
result = (-2147483648 / y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 663: RHS constant
x = -2147483648;
result = (x / 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 664: both arguments variables
x = -2147483648;
y = -1;
result = (x / y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 665: both arguments constants
result = (-2147483648 / -1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 666: LHS constant
y = -1;
result = (-2147483648 / y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 667: RHS constant
x = -2147483648;
result = (x / -1)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 668: both arguments variables
x = -2147483648;
y = 2;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 669: both arguments constants
result = (-2147483648 / 2)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 670: LHS constant
y = 2;
result = (-2147483648 / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 671: RHS constant
x = -2147483648;
result = (x / 2)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 672: both arguments variables
x = -2147483648;
y = -2;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 673: both arguments constants
result = (-2147483648 / -2)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 674: LHS constant
y = -2;
result = (-2147483648 / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 675: RHS constant
x = -2147483648;
result = (x / -2)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 676: both arguments variables
x = -2147483648;
y = 4;
result = (x / y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 677: both arguments constants
result = (-2147483648 / 4)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 678: LHS constant
y = 4;
result = (-2147483648 / y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 679: RHS constant
x = -2147483648;
result = (x / 4)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 680: both arguments variables
x = -2147483648;
y = -4;
result = (x / y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 681: both arguments constants
result = (-2147483648 / -4)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 682: LHS constant
y = -4;
result = (-2147483648 / y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 683: RHS constant
x = -2147483648;
result = (x / -4)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 684: both arguments variables
x = -2147483648;
y = 8;
result = (x / y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 685: both arguments constants
result = (-2147483648 / 8)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 686: LHS constant
y = 8;
result = (-2147483648 / y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 687: RHS constant
x = -2147483648;
result = (x / 8)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 688: both arguments variables
x = -2147483648;
y = -8;
result = (x / y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 689: both arguments constants
result = (-2147483648 / -8)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 690: LHS constant
y = -8;
result = (-2147483648 / y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 691: RHS constant
x = -2147483648;
result = (x / -8)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 692: both arguments variables
x = -2147483648;
y = 1073741824;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 693: both arguments constants
result = (-2147483648 / 1073741824)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 694: LHS constant
y = 1073741824;
result = (-2147483648 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 695: RHS constant
x = -2147483648;
result = (x / 1073741824)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 696: both arguments variables
x = -2147483648;
y = (-0x3fffffff-1);
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 697: both arguments constants
result = (-2147483648 / (-0x3fffffff-1))
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 698: LHS constant
y = (-0x3fffffff-1);
result = (-2147483648 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 699: RHS constant
x = -2147483648;
result = (x / (-0x3fffffff-1))
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test7() {
var x;
var y;
var result;
var check;
// Test 700: both arguments variables
x = -2147483648;
y = 2147483648;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 701: both arguments constants
result = (-2147483648 / 2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 702: LHS constant
y = 2147483648;
result = (-2147483648 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 703: RHS constant
x = -2147483648;
result = (x / 2147483648)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 704: both arguments variables
x = -2147483648;
y = -2147483648;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 705: both arguments constants
result = (-2147483648 / -2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 706: LHS constant
y = -2147483648;
result = (-2147483648 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 707: RHS constant
x = -2147483648;
result = (x / -2147483648)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 708: both arguments variables
x = -2147483649;
y = 1;
result = (x / y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 709: both arguments constants
result = (-2147483649 / 1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 710: LHS constant
y = 1;
result = (-2147483649 / y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 711: RHS constant
x = -2147483649;
result = (x / 1)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 712: both arguments variables
x = -2147483649;
y = -1;
result = (x / y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 713: both arguments constants
result = (-2147483649 / -1)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 714: LHS constant
y = -1;
result = (-2147483649 / y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 715: RHS constant
x = -2147483649;
result = (x / -1)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 716: both arguments variables
x = -2147483649;
y = 3;
result = (x / y);
check = -715827883;
if(result != check) { fail(test, check, result); } ++test; 

// Test 717: both arguments constants
result = (-2147483649 / 3)
check = -715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 718: LHS constant
y = 3;
result = (-2147483649 / y)
check = -715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 719: RHS constant
x = -2147483649;
result = (x / 3)
check = -715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 720: both arguments variables
x = -2147483649;
y = -3;
result = (x / y);
check = 715827883;
if(result != check) { fail(test, check, result); } ++test; 

// Test 721: both arguments constants
result = (-2147483649 / -3)
check = 715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 722: LHS constant
y = -3;
result = (-2147483649 / y)
check = 715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 723: RHS constant
x = -2147483649;
result = (x / -3)
check = 715827883
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 724: both arguments variables
x = -2147483649;
y = 2147483649;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 725: both arguments constants
result = (-2147483649 / 2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 726: LHS constant
y = 2147483649;
result = (-2147483649 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 727: RHS constant
x = -2147483649;
result = (x / 2147483649)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 728: both arguments variables
x = -2147483649;
y = -2147483649;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 729: both arguments constants
result = (-2147483649 / -2147483649)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 730: LHS constant
y = -2147483649;
result = (-2147483649 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 731: RHS constant
x = -2147483649;
result = (x / -2147483649)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 732: both arguments variables
x = -2147483650;
y = 1;
result = (x / y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 733: both arguments constants
result = (-2147483650 / 1)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 734: LHS constant
y = 1;
result = (-2147483650 / y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 735: RHS constant
x = -2147483650;
result = (x / 1)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 736: both arguments variables
x = -2147483650;
y = -1;
result = (x / y);
check = 2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 737: both arguments constants
result = (-2147483650 / -1)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 738: LHS constant
y = -1;
result = (-2147483650 / y)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 739: RHS constant
x = -2147483650;
result = (x / -1)
check = 2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 740: both arguments variables
x = -2147483650;
y = 2;
result = (x / y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 741: both arguments constants
result = (-2147483650 / 2)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 742: LHS constant
y = 2;
result = (-2147483650 / y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 743: RHS constant
x = -2147483650;
result = (x / 2)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 744: both arguments variables
x = -2147483650;
y = -2;
result = (x / y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 745: both arguments constants
result = (-2147483650 / -2)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 746: LHS constant
y = -2;
result = (-2147483650 / y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 747: RHS constant
x = -2147483650;
result = (x / -2)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 748: both arguments variables
x = -2147483650;
y = 1073741825;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 749: both arguments constants
result = (-2147483650 / 1073741825)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 750: LHS constant
y = 1073741825;
result = (-2147483650 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 751: RHS constant
x = -2147483650;
result = (x / 1073741825)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 752: both arguments variables
x = -2147483650;
y = -1073741825;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 753: both arguments constants
result = (-2147483650 / -1073741825)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 754: LHS constant
y = -1073741825;
result = (-2147483650 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 755: RHS constant
x = -2147483650;
result = (x / -1073741825)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 756: both arguments variables
x = -2147483650;
y = -2147483650;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 757: both arguments constants
result = (-2147483650 / -2147483650)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 758: LHS constant
y = -2147483650;
result = (-2147483650 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 759: RHS constant
x = -2147483650;
result = (x / -2147483650)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 760: both arguments variables
x = 4294967295;
y = 1;
result = (x / y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 761: both arguments constants
result = (4294967295 / 1)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 762: LHS constant
y = 1;
result = (4294967295 / y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 763: RHS constant
x = 4294967295;
result = (x / 1)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 764: both arguments variables
x = 4294967295;
y = -1;
result = (x / y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 765: both arguments constants
result = (4294967295 / -1)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 766: LHS constant
y = -1;
result = (4294967295 / y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 767: RHS constant
x = 4294967295;
result = (x / -1)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 768: both arguments variables
x = 4294967295;
y = 3;
result = (x / y);
check = 1431655765;
if(result != check) { fail(test, check, result); } ++test; 

// Test 769: both arguments constants
result = (4294967295 / 3)
check = 1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 770: LHS constant
y = 3;
result = (4294967295 / y)
check = 1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 771: RHS constant
x = 4294967295;
result = (x / 3)
check = 1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 772: both arguments variables
x = 4294967295;
y = -3;
result = (x / y);
check = -1431655765;
if(result != check) { fail(test, check, result); } ++test; 

// Test 773: both arguments constants
result = (4294967295 / -3)
check = -1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 774: LHS constant
y = -3;
result = (4294967295 / y)
check = -1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 775: RHS constant
x = 4294967295;
result = (x / -3)
check = -1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 776: both arguments variables
x = 4294967295;
y = 4294967295;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 777: both arguments constants
result = (4294967295 / 4294967295)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 778: LHS constant
y = 4294967295;
result = (4294967295 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 779: RHS constant
x = 4294967295;
result = (x / 4294967295)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 780: both arguments variables
x = 4294967295;
y = -4294967295;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 781: both arguments constants
result = (4294967295 / -4294967295)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 782: LHS constant
y = -4294967295;
result = (4294967295 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 783: RHS constant
x = 4294967295;
result = (x / -4294967295)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 784: both arguments variables
x = 4294967296;
y = 1;
result = (x / y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 785: both arguments constants
result = (4294967296 / 1)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 786: LHS constant
y = 1;
result = (4294967296 / y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 787: RHS constant
x = 4294967296;
result = (x / 1)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 788: both arguments variables
x = 4294967296;
y = -1;
result = (x / y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 789: both arguments constants
result = (4294967296 / -1)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 790: LHS constant
y = -1;
result = (4294967296 / y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 791: RHS constant
x = 4294967296;
result = (x / -1)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 792: both arguments variables
x = 4294967296;
y = 2;
result = (x / y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 793: both arguments constants
result = (4294967296 / 2)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 794: LHS constant
y = 2;
result = (4294967296 / y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 795: RHS constant
x = 4294967296;
result = (x / 2)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 796: both arguments variables
x = 4294967296;
y = -2;
result = (x / y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 797: both arguments constants
result = (4294967296 / -2)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 798: LHS constant
y = -2;
result = (4294967296 / y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 799: RHS constant
x = 4294967296;
result = (x / -2)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test8() {
var x;
var y;
var result;
var check;
// Test 800: both arguments variables
x = 4294967296;
y = 4;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 801: both arguments constants
result = (4294967296 / 4)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 802: LHS constant
y = 4;
result = (4294967296 / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 803: RHS constant
x = 4294967296;
result = (x / 4)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 804: both arguments variables
x = 4294967296;
y = -4;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 805: both arguments constants
result = (4294967296 / -4)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 806: LHS constant
y = -4;
result = (4294967296 / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 807: RHS constant
x = 4294967296;
result = (x / -4)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 808: both arguments variables
x = 4294967296;
y = 8;
result = (x / y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 809: both arguments constants
result = (4294967296 / 8)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 810: LHS constant
y = 8;
result = (4294967296 / y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 811: RHS constant
x = 4294967296;
result = (x / 8)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 812: both arguments variables
x = 4294967296;
y = -8;
result = (x / y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 813: both arguments constants
result = (4294967296 / -8)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 814: LHS constant
y = -8;
result = (4294967296 / y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 815: RHS constant
x = 4294967296;
result = (x / -8)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 816: both arguments variables
x = 4294967296;
y = 1073741824;
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 817: both arguments constants
result = (4294967296 / 1073741824)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 818: LHS constant
y = 1073741824;
result = (4294967296 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 819: RHS constant
x = 4294967296;
result = (x / 1073741824)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 820: both arguments variables
x = 4294967296;
y = (-0x3fffffff-1);
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 821: both arguments constants
result = (4294967296 / (-0x3fffffff-1))
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 822: LHS constant
y = (-0x3fffffff-1);
result = (4294967296 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 823: RHS constant
x = 4294967296;
result = (x / (-0x3fffffff-1))
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 824: both arguments variables
x = 4294967296;
y = 2147483648;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 825: both arguments constants
result = (4294967296 / 2147483648)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 826: LHS constant
y = 2147483648;
result = (4294967296 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 827: RHS constant
x = 4294967296;
result = (x / 2147483648)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 828: both arguments variables
x = 4294967296;
y = -2147483648;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 829: both arguments constants
result = (4294967296 / -2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 830: LHS constant
y = -2147483648;
result = (4294967296 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 831: RHS constant
x = 4294967296;
result = (x / -2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 832: both arguments variables
x = 4294967296;
y = 4294967296;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 833: both arguments constants
result = (4294967296 / 4294967296)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 834: LHS constant
y = 4294967296;
result = (4294967296 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 835: RHS constant
x = 4294967296;
result = (x / 4294967296)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 836: both arguments variables
x = 4294967296;
y = -4294967296;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 837: both arguments constants
result = (4294967296 / -4294967296)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 838: LHS constant
y = -4294967296;
result = (4294967296 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 839: RHS constant
x = 4294967296;
result = (x / -4294967296)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 840: both arguments variables
x = -4294967295;
y = 1;
result = (x / y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 841: both arguments constants
result = (-4294967295 / 1)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 842: LHS constant
y = 1;
result = (-4294967295 / y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 843: RHS constant
x = -4294967295;
result = (x / 1)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 844: both arguments variables
x = -4294967295;
y = -1;
result = (x / y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 845: both arguments constants
result = (-4294967295 / -1)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 846: LHS constant
y = -1;
result = (-4294967295 / y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 847: RHS constant
x = -4294967295;
result = (x / -1)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 848: both arguments variables
x = -4294967295;
y = 3;
result = (x / y);
check = -1431655765;
if(result != check) { fail(test, check, result); } ++test; 

// Test 849: both arguments constants
result = (-4294967295 / 3)
check = -1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 850: LHS constant
y = 3;
result = (-4294967295 / y)
check = -1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 851: RHS constant
x = -4294967295;
result = (x / 3)
check = -1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 852: both arguments variables
x = -4294967295;
y = -3;
result = (x / y);
check = 1431655765;
if(result != check) { fail(test, check, result); } ++test; 

// Test 853: both arguments constants
result = (-4294967295 / -3)
check = 1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 854: LHS constant
y = -3;
result = (-4294967295 / y)
check = 1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 855: RHS constant
x = -4294967295;
result = (x / -3)
check = 1431655765
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 856: both arguments variables
x = -4294967295;
y = 4294967295;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 857: both arguments constants
result = (-4294967295 / 4294967295)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 858: LHS constant
y = 4294967295;
result = (-4294967295 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 859: RHS constant
x = -4294967295;
result = (x / 4294967295)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 860: both arguments variables
x = -4294967295;
y = -4294967295;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 861: both arguments constants
result = (-4294967295 / -4294967295)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 862: LHS constant
y = -4294967295;
result = (-4294967295 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 863: RHS constant
x = -4294967295;
result = (x / -4294967295)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 864: both arguments variables
x = -4294967296;
y = 1;
result = (x / y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 865: both arguments constants
result = (-4294967296 / 1)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 866: LHS constant
y = 1;
result = (-4294967296 / y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 867: RHS constant
x = -4294967296;
result = (x / 1)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 868: both arguments variables
x = -4294967296;
y = -1;
result = (x / y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 869: both arguments constants
result = (-4294967296 / -1)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 870: LHS constant
y = -1;
result = (-4294967296 / y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 871: RHS constant
x = -4294967296;
result = (x / -1)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 872: both arguments variables
x = -4294967296;
y = 2;
result = (x / y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 873: both arguments constants
result = (-4294967296 / 2)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 874: LHS constant
y = 2;
result = (-4294967296 / y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 875: RHS constant
x = -4294967296;
result = (x / 2)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 876: both arguments variables
x = -4294967296;
y = -2;
result = (x / y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 877: both arguments constants
result = (-4294967296 / -2)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 878: LHS constant
y = -2;
result = (-4294967296 / y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 879: RHS constant
x = -4294967296;
result = (x / -2)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 880: both arguments variables
x = -4294967296;
y = 4;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 881: both arguments constants
result = (-4294967296 / 4)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 882: LHS constant
y = 4;
result = (-4294967296 / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 883: RHS constant
x = -4294967296;
result = (x / 4)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 884: both arguments variables
x = -4294967296;
y = -4;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 885: both arguments constants
result = (-4294967296 / -4)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 886: LHS constant
y = -4;
result = (-4294967296 / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 887: RHS constant
x = -4294967296;
result = (x / -4)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 888: both arguments variables
x = -4294967296;
y = 8;
result = (x / y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 889: both arguments constants
result = (-4294967296 / 8)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 890: LHS constant
y = 8;
result = (-4294967296 / y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 891: RHS constant
x = -4294967296;
result = (x / 8)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 892: both arguments variables
x = -4294967296;
y = -8;
result = (x / y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 893: both arguments constants
result = (-4294967296 / -8)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 894: LHS constant
y = -8;
result = (-4294967296 / y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 895: RHS constant
x = -4294967296;
result = (x / -8)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 896: both arguments variables
x = -4294967296;
y = 1073741824;
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 897: both arguments constants
result = (-4294967296 / 1073741824)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 898: LHS constant
y = 1073741824;
result = (-4294967296 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 899: RHS constant
x = -4294967296;
result = (x / 1073741824)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test9() {
var x;
var y;
var result;
var check;
// Test 900: both arguments variables
x = -4294967296;
y = (-0x3fffffff-1);
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 901: both arguments constants
result = (-4294967296 / (-0x3fffffff-1))
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 902: LHS constant
y = (-0x3fffffff-1);
result = (-4294967296 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 903: RHS constant
x = -4294967296;
result = (x / (-0x3fffffff-1))
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 904: both arguments variables
x = -4294967296;
y = 2147483648;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 905: both arguments constants
result = (-4294967296 / 2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 906: LHS constant
y = 2147483648;
result = (-4294967296 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 907: RHS constant
x = -4294967296;
result = (x / 2147483648)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 908: both arguments variables
x = -4294967296;
y = -2147483648;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 909: both arguments constants
result = (-4294967296 / -2147483648)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 910: LHS constant
y = -2147483648;
result = (-4294967296 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 911: RHS constant
x = -4294967296;
result = (x / -2147483648)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 912: both arguments variables
x = -4294967296;
y = 4294967296;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 913: both arguments constants
result = (-4294967296 / 4294967296)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 914: LHS constant
y = 4294967296;
result = (-4294967296 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 915: RHS constant
x = -4294967296;
result = (x / 4294967296)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 916: both arguments variables
x = -4294967296;
y = -4294967296;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 917: both arguments constants
result = (-4294967296 / -4294967296)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 918: LHS constant
y = -4294967296;
result = (-4294967296 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 919: RHS constant
x = -4294967296;
result = (x / -4294967296)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 920: both arguments variables
x = 0;
y = 684451;
result = (x / y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 921: both arguments constants
result = (0 / 684451)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 922: LHS constant
y = 684451;
result = (0 / y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 923: RHS constant
x = 0;
result = (x / 684451)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 924: both arguments variables
x = 684451;
y = 684451;
result = (x / y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 925: both arguments constants
result = (684451 / 684451)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 926: LHS constant
y = 684451;
result = (684451 / y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 927: RHS constant
x = 684451;
result = (x / 684451)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 928: both arguments variables
x = -684451;
y = 684451;
result = (x / y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 929: both arguments constants
result = (-684451 / 684451)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 930: LHS constant
y = 684451;
result = (-684451 / y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 931: RHS constant
x = -684451;
result = (x / 684451)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 932: both arguments variables
x = 1368902;
y = 684451;
result = (x / y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 933: both arguments constants
result = (1368902 / 684451)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 934: LHS constant
y = 684451;
result = (1368902 / y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 935: RHS constant
x = 1368902;
result = (x / 684451)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 936: both arguments variables
x = -1368902;
y = 684451;
result = (x / y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 937: both arguments constants
result = (-1368902 / 684451)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 938: LHS constant
y = 684451;
result = (-1368902 / y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 939: RHS constant
x = -1368902;
result = (x / 684451)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 940: both arguments variables
x = 2053353;
y = 684451;
result = (x / y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 941: both arguments constants
result = (2053353 / 684451)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 942: LHS constant
y = 684451;
result = (2053353 / y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 943: RHS constant
x = 2053353;
result = (x / 684451)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 944: both arguments variables
x = -2053353;
y = 684451;
result = (x / y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 945: both arguments constants
result = (-2053353 / 684451)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 946: LHS constant
y = 684451;
result = (-2053353 / y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 947: RHS constant
x = -2053353;
result = (x / 684451)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 948: both arguments variables
x = 2737804;
y = 684451;
result = (x / y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 949: both arguments constants
result = (2737804 / 684451)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 950: LHS constant
y = 684451;
result = (2737804 / y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 951: RHS constant
x = 2737804;
result = (x / 684451)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 952: both arguments variables
x = -2737804;
y = 684451;
result = (x / y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 953: both arguments constants
result = (-2737804 / 684451)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 954: LHS constant
y = 684451;
result = (-2737804 / y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 955: RHS constant
x = -2737804;
result = (x / 684451)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 956: both arguments variables
x = 5475608;
y = 684451;
result = (x / y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 957: both arguments constants
result = (5475608 / 684451)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 958: LHS constant
y = 684451;
result = (5475608 / y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 959: RHS constant
x = 5475608;
result = (x / 684451)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 960: both arguments variables
x = -5475608;
y = 684451;
result = (x / y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 961: both arguments constants
result = (-5475608 / 684451)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 962: LHS constant
y = 684451;
result = (-5475608 / y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 963: RHS constant
x = -5475608;
result = (x / 684451)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 964: both arguments variables
x = 734923663809722;
y = 684451;
result = (x / y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 965: both arguments constants
result = (734923663809722 / 684451)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 966: LHS constant
y = 684451;
result = (734923663809722 / y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 967: RHS constant
x = 734923663809722;
result = (x / 684451)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 968: both arguments variables
x = 734923664494173;
y = 684451;
result = (x / y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 969: both arguments constants
result = (734923664494173 / 684451)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 970: LHS constant
y = 684451;
result = (734923664494173 / y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 971: RHS constant
x = 734923664494173;
result = (x / 684451)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 972: both arguments variables
x = 734923665178624;
y = 684451;
result = (x / y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 973: both arguments constants
result = (734923665178624 / 684451)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 974: LHS constant
y = 684451;
result = (734923665178624 / y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 975: RHS constant
x = 734923665178624;
result = (x / 684451)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 976: both arguments variables
x = 734923665863075;
y = 684451;
result = (x / y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 977: both arguments constants
result = (734923665863075 / 684451)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 978: LHS constant
y = 684451;
result = (734923665863075 / y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 979: RHS constant
x = 734923665863075;
result = (x / 684451)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 980: both arguments variables
x = -734923664494173;
y = 684451;
result = (x / y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 981: both arguments constants
result = (-734923664494173 / 684451)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 982: LHS constant
y = 684451;
result = (-734923664494173 / y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 983: RHS constant
x = -734923664494173;
result = (x / 684451)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 984: both arguments variables
x = -734923665178624;
y = 684451;
result = (x / y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 985: both arguments constants
result = (-734923665178624 / 684451)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 986: LHS constant
y = 684451;
result = (-734923665178624 / y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 987: RHS constant
x = -734923665178624;
result = (x / 684451)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 988: both arguments variables
x = -734923665863075;
y = 684451;
result = (x / y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 989: both arguments constants
result = (-734923665863075 / 684451)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 990: LHS constant
y = 684451;
result = (-734923665863075 / y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 991: RHS constant
x = -734923665863075;
result = (x / 684451)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 992: both arguments variables
x = -734923666547526;
y = 684451;
result = (x / y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 993: both arguments constants
result = (-734923666547526 / 684451)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 994: LHS constant
y = 684451;
result = (-734923666547526 / y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 995: RHS constant
x = -734923666547526;
result = (x / 684451)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 996: both arguments variables
x = 1469847328988346;
y = 684451;
result = (x / y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 997: both arguments constants
result = (1469847328988346 / 684451)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 998: LHS constant
y = 684451;
result = (1469847328988346 / y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 999: RHS constant
x = 1469847328988346;
result = (x / 684451)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test10() {
var x;
var y;
var result;
var check;
// Test 1000: both arguments variables
x = 1469847329672797;
y = 684451;
result = (x / y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1001: both arguments constants
result = (1469847329672797 / 684451)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1002: LHS constant
y = 684451;
result = (1469847329672797 / y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1003: RHS constant
x = 1469847329672797;
result = (x / 684451)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1004: both arguments variables
x = 1469847330357248;
y = 684451;
result = (x / y);
check = 2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1005: both arguments constants
result = (1469847330357248 / 684451)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1006: LHS constant
y = 684451;
result = (1469847330357248 / y)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1007: RHS constant
x = 1469847330357248;
result = (x / 684451)
check = 2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1008: both arguments variables
x = 1469847331041699;
y = 684451;
result = (x / y);
check = 2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1009: both arguments constants
result = (1469847331041699 / 684451)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1010: LHS constant
y = 684451;
result = (1469847331041699 / y)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1011: RHS constant
x = 1469847331041699;
result = (x / 684451)
check = 2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1012: both arguments variables
x = -1469847329672797;
y = 684451;
result = (x / y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1013: both arguments constants
result = (-1469847329672797 / 684451)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1014: LHS constant
y = 684451;
result = (-1469847329672797 / y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1015: RHS constant
x = -1469847329672797;
result = (x / 684451)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1016: both arguments variables
x = -1469847330357248;
y = 684451;
result = (x / y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1017: both arguments constants
result = (-1469847330357248 / 684451)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1018: LHS constant
y = 684451;
result = (-1469847330357248 / y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1019: RHS constant
x = -1469847330357248;
result = (x / 684451)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1020: both arguments variables
x = -1469847331041699;
y = 684451;
result = (x / y);
check = -2147483649;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1021: both arguments constants
result = (-1469847331041699 / 684451)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1022: LHS constant
y = 684451;
result = (-1469847331041699 / y)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1023: RHS constant
x = -1469847331041699;
result = (x / 684451)
check = -2147483649
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1024: both arguments variables
x = -1469847331726150;
y = 684451;
result = (x / y);
check = -2147483650;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1025: both arguments constants
result = (-1469847331726150 / 684451)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1026: LHS constant
y = 684451;
result = (-1469847331726150 / y)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1027: RHS constant
x = -1469847331726150;
result = (x / 684451)
check = -2147483650
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1028: both arguments variables
x = 2939694660030045;
y = 684451;
result = (x / y);
check = 4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1029: both arguments constants
result = (2939694660030045 / 684451)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1030: LHS constant
y = 684451;
result = (2939694660030045 / y)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1031: RHS constant
x = 2939694660030045;
result = (x / 684451)
check = 4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1032: both arguments variables
x = 2939694660714496;
y = 684451;
result = (x / y);
check = 4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1033: both arguments constants
result = (2939694660714496 / 684451)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1034: LHS constant
y = 684451;
result = (2939694660714496 / y)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1035: RHS constant
x = 2939694660714496;
result = (x / 684451)
check = 4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1036: both arguments variables
x = -2939694660030045;
y = 684451;
result = (x / y);
check = -4294967295;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1037: both arguments constants
result = (-2939694660030045 / 684451)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1038: LHS constant
y = 684451;
result = (-2939694660030045 / y)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1039: RHS constant
x = -2939694660030045;
result = (x / 684451)
check = -4294967295
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1040: both arguments variables
x = -2939694660714496;
y = 684451;
result = (x / y);
check = -4294967296;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1041: both arguments constants
result = (-2939694660714496 / 684451)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1042: LHS constant
y = 684451;
result = (-2939694660714496 / y)
check = -4294967296
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1043: RHS constant
x = -2939694660714496;
result = (x / 684451)
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
WScript.Echo("done");
