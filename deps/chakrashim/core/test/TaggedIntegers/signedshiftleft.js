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
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1: both arguments constants
result = (0 << 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2: LHS constant
y = 0;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3: RHS constant
x = 0;
result = (x << 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 4: both arguments variables
x = 1;
y = 0;
result = (x << y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 5: both arguments constants
result = (1 << 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 6: LHS constant
y = 0;
result = (1 << y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 7: RHS constant
x = 1;
result = (x << 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 8: both arguments variables
x = -1;
y = 0;
result = (x << y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 9: both arguments constants
result = (-1 << 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 10: LHS constant
y = 0;
result = (-1 << y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 11: RHS constant
x = -1;
result = (x << 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 12: both arguments variables
x = 2;
y = 0;
result = (x << y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 13: both arguments constants
result = (2 << 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 14: LHS constant
y = 0;
result = (2 << y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 15: RHS constant
x = 2;
result = (x << 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 16: both arguments variables
x = -2;
y = 0;
result = (x << y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 17: both arguments constants
result = (-2 << 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 18: LHS constant
y = 0;
result = (-2 << y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 19: RHS constant
x = -2;
result = (x << 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 20: both arguments variables
x = 3;
y = 0;
result = (x << y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 21: both arguments constants
result = (3 << 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 22: LHS constant
y = 0;
result = (3 << y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 23: RHS constant
x = 3;
result = (x << 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 24: both arguments variables
x = -3;
y = 0;
result = (x << y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 25: both arguments constants
result = (-3 << 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 26: LHS constant
y = 0;
result = (-3 << y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 27: RHS constant
x = -3;
result = (x << 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 28: both arguments variables
x = 4;
y = 0;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 29: both arguments constants
result = (4 << 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 30: LHS constant
y = 0;
result = (4 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 31: RHS constant
x = 4;
result = (x << 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 32: both arguments variables
x = -4;
y = 0;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 33: both arguments constants
result = (-4 << 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 34: LHS constant
y = 0;
result = (-4 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 35: RHS constant
x = -4;
result = (x << 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 36: both arguments variables
x = 8;
y = 0;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 37: both arguments constants
result = (8 << 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 38: LHS constant
y = 0;
result = (8 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 39: RHS constant
x = 8;
result = (x << 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 40: both arguments variables
x = -8;
y = 0;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 41: both arguments constants
result = (-8 << 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 42: LHS constant
y = 0;
result = (-8 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 43: RHS constant
x = -8;
result = (x << 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 44: both arguments variables
x = 1073741822;
y = 0;
result = (x << y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 45: both arguments constants
result = (1073741822 << 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 46: LHS constant
y = 0;
result = (1073741822 << y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 47: RHS constant
x = 1073741822;
result = (x << 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 48: both arguments variables
x = 1073741823;
y = 0;
result = (x << y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 49: both arguments constants
result = (1073741823 << 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 50: LHS constant
y = 0;
result = (1073741823 << y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 51: RHS constant
x = 1073741823;
result = (x << 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 52: both arguments variables
x = 1073741824;
y = 0;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 53: both arguments constants
result = (1073741824 << 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 54: LHS constant
y = 0;
result = (1073741824 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 55: RHS constant
x = 1073741824;
result = (x << 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 56: both arguments variables
x = 1073741825;
y = 0;
result = (x << y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 57: both arguments constants
result = (1073741825 << 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 58: LHS constant
y = 0;
result = (1073741825 << y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 59: RHS constant
x = 1073741825;
result = (x << 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 60: both arguments variables
x = -1073741823;
y = 0;
result = (x << y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 61: both arguments constants
result = (-1073741823 << 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 62: LHS constant
y = 0;
result = (-1073741823 << y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 63: RHS constant
x = -1073741823;
result = (x << 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 64: both arguments variables
x = (-0x3fffffff-1);
y = 0;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 65: both arguments constants
result = ((-0x3fffffff-1) << 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 66: LHS constant
y = 0;
result = ((-0x3fffffff-1) << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 67: RHS constant
x = (-0x3fffffff-1);
result = (x << 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 68: both arguments variables
x = -1073741825;
y = 0;
result = (x << y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 69: both arguments constants
result = (-1073741825 << 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 70: LHS constant
y = 0;
result = (-1073741825 << y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 71: RHS constant
x = -1073741825;
result = (x << 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 72: both arguments variables
x = -1073741826;
y = 0;
result = (x << y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 73: both arguments constants
result = (-1073741826 << 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 74: LHS constant
y = 0;
result = (-1073741826 << y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 75: RHS constant
x = -1073741826;
result = (x << 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 76: both arguments variables
x = 2147483646;
y = 0;
result = (x << y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 77: both arguments constants
result = (2147483646 << 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 78: LHS constant
y = 0;
result = (2147483646 << y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 79: RHS constant
x = 2147483646;
result = (x << 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 80: both arguments variables
x = 2147483647;
y = 0;
result = (x << y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 81: both arguments constants
result = (2147483647 << 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 82: LHS constant
y = 0;
result = (2147483647 << y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 83: RHS constant
x = 2147483647;
result = (x << 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 84: both arguments variables
x = 2147483648;
y = 0;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 85: both arguments constants
result = (2147483648 << 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 86: LHS constant
y = 0;
result = (2147483648 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 87: RHS constant
x = 2147483648;
result = (x << 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 88: both arguments variables
x = 2147483649;
y = 0;
result = (x << y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 89: both arguments constants
result = (2147483649 << 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 90: LHS constant
y = 0;
result = (2147483649 << y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 91: RHS constant
x = 2147483649;
result = (x << 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 92: both arguments variables
x = -2147483647;
y = 0;
result = (x << y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 93: both arguments constants
result = (-2147483647 << 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 94: LHS constant
y = 0;
result = (-2147483647 << y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 95: RHS constant
x = -2147483647;
result = (x << 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 96: both arguments variables
x = -2147483648;
y = 0;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 97: both arguments constants
result = (-2147483648 << 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 98: LHS constant
y = 0;
result = (-2147483648 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 99: RHS constant
x = -2147483648;
result = (x << 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test1() {
var x;
var y;
var result;
var check;
// Test 100: both arguments variables
x = -2147483649;
y = 0;
result = (x << y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 101: both arguments constants
result = (-2147483649 << 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 102: LHS constant
y = 0;
result = (-2147483649 << y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 103: RHS constant
x = -2147483649;
result = (x << 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 104: both arguments variables
x = -2147483650;
y = 0;
result = (x << y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 105: both arguments constants
result = (-2147483650 << 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 106: LHS constant
y = 0;
result = (-2147483650 << y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 107: RHS constant
x = -2147483650;
result = (x << 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 108: both arguments variables
x = 4294967295;
y = 0;
result = (x << y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 109: both arguments constants
result = (4294967295 << 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 110: LHS constant
y = 0;
result = (4294967295 << y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 111: RHS constant
x = 4294967295;
result = (x << 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 112: both arguments variables
x = 4294967296;
y = 0;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 113: both arguments constants
result = (4294967296 << 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 114: LHS constant
y = 0;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 115: RHS constant
x = 4294967296;
result = (x << 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 116: both arguments variables
x = -4294967295;
y = 0;
result = (x << y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 117: both arguments constants
result = (-4294967295 << 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 118: LHS constant
y = 0;
result = (-4294967295 << y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 119: RHS constant
x = -4294967295;
result = (x << 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 120: both arguments variables
x = -4294967296;
y = 0;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 121: both arguments constants
result = (-4294967296 << 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 122: LHS constant
y = 0;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 123: RHS constant
x = -4294967296;
result = (x << 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 124: both arguments variables
x = 0;
y = 1;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 125: both arguments constants
result = (0 << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 126: LHS constant
y = 1;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 127: RHS constant
x = 0;
result = (x << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 128: both arguments variables
x = 1;
y = 1;
result = (x << y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 129: both arguments constants
result = (1 << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 130: LHS constant
y = 1;
result = (1 << y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 131: RHS constant
x = 1;
result = (x << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 132: both arguments variables
x = -1;
y = 1;
result = (x << y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 133: both arguments constants
result = (-1 << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 134: LHS constant
y = 1;
result = (-1 << y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 135: RHS constant
x = -1;
result = (x << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 136: both arguments variables
x = 2;
y = 1;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 137: both arguments constants
result = (2 << 1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 138: LHS constant
y = 1;
result = (2 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 139: RHS constant
x = 2;
result = (x << 1)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 140: both arguments variables
x = -2;
y = 1;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 141: both arguments constants
result = (-2 << 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 142: LHS constant
y = 1;
result = (-2 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 143: RHS constant
x = -2;
result = (x << 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 144: both arguments variables
x = 3;
y = 1;
result = (x << y);
check = 6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 145: both arguments constants
result = (3 << 1)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 146: LHS constant
y = 1;
result = (3 << y)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 147: RHS constant
x = 3;
result = (x << 1)
check = 6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 148: both arguments variables
x = -3;
y = 1;
result = (x << y);
check = -6;
if(result != check) { fail(test, check, result); } ++test; 

// Test 149: both arguments constants
result = (-3 << 1)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 150: LHS constant
y = 1;
result = (-3 << y)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 151: RHS constant
x = -3;
result = (x << 1)
check = -6
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 152: both arguments variables
x = 4;
y = 1;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 153: both arguments constants
result = (4 << 1)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 154: LHS constant
y = 1;
result = (4 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 155: RHS constant
x = 4;
result = (x << 1)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 156: both arguments variables
x = -4;
y = 1;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 157: both arguments constants
result = (-4 << 1)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 158: LHS constant
y = 1;
result = (-4 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 159: RHS constant
x = -4;
result = (x << 1)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 160: both arguments variables
x = 8;
y = 1;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 161: both arguments constants
result = (8 << 1)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 162: LHS constant
y = 1;
result = (8 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 163: RHS constant
x = 8;
result = (x << 1)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 164: both arguments variables
x = -8;
y = 1;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 165: both arguments constants
result = (-8 << 1)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 166: LHS constant
y = 1;
result = (-8 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 167: RHS constant
x = -8;
result = (x << 1)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 168: both arguments variables
x = 1073741822;
y = 1;
result = (x << y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 169: both arguments constants
result = (1073741822 << 1)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 170: LHS constant
y = 1;
result = (1073741822 << y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 171: RHS constant
x = 1073741822;
result = (x << 1)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 172: both arguments variables
x = 1073741823;
y = 1;
result = (x << y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 173: both arguments constants
result = (1073741823 << 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 174: LHS constant
y = 1;
result = (1073741823 << y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 175: RHS constant
x = 1073741823;
result = (x << 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 176: both arguments variables
x = 1073741824;
y = 1;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 177: both arguments constants
result = (1073741824 << 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 178: LHS constant
y = 1;
result = (1073741824 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 179: RHS constant
x = 1073741824;
result = (x << 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 180: both arguments variables
x = 1073741825;
y = 1;
result = (x << y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 181: both arguments constants
result = (1073741825 << 1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 182: LHS constant
y = 1;
result = (1073741825 << y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 183: RHS constant
x = 1073741825;
result = (x << 1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 184: both arguments variables
x = -1073741823;
y = 1;
result = (x << y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 185: both arguments constants
result = (-1073741823 << 1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 186: LHS constant
y = 1;
result = (-1073741823 << y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 187: RHS constant
x = -1073741823;
result = (x << 1)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 188: both arguments variables
x = (-0x3fffffff-1);
y = 1;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 189: both arguments constants
result = ((-0x3fffffff-1) << 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 190: LHS constant
y = 1;
result = ((-0x3fffffff-1) << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 191: RHS constant
x = (-0x3fffffff-1);
result = (x << 1)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 192: both arguments variables
x = -1073741825;
y = 1;
result = (x << y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 193: both arguments constants
result = (-1073741825 << 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 194: LHS constant
y = 1;
result = (-1073741825 << y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 195: RHS constant
x = -1073741825;
result = (x << 1)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 196: both arguments variables
x = -1073741826;
y = 1;
result = (x << y);
check = 2147483644;
if(result != check) { fail(test, check, result); } ++test; 

// Test 197: both arguments constants
result = (-1073741826 << 1)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 198: LHS constant
y = 1;
result = (-1073741826 << y)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 199: RHS constant
x = -1073741826;
result = (x << 1)
check = 2147483644
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test2() {
var x;
var y;
var result;
var check;
// Test 200: both arguments variables
x = 2147483646;
y = 1;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 201: both arguments constants
result = (2147483646 << 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 202: LHS constant
y = 1;
result = (2147483646 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 203: RHS constant
x = 2147483646;
result = (x << 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 204: both arguments variables
x = 2147483647;
y = 1;
result = (x << y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 205: both arguments constants
result = (2147483647 << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 206: LHS constant
y = 1;
result = (2147483647 << y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 207: RHS constant
x = 2147483647;
result = (x << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 208: both arguments variables
x = 2147483648;
y = 1;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 209: both arguments constants
result = (2147483648 << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 210: LHS constant
y = 1;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 211: RHS constant
x = 2147483648;
result = (x << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 212: both arguments variables
x = 2147483649;
y = 1;
result = (x << y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 213: both arguments constants
result = (2147483649 << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 214: LHS constant
y = 1;
result = (2147483649 << y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 215: RHS constant
x = 2147483649;
result = (x << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 216: both arguments variables
x = -2147483647;
y = 1;
result = (x << y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 217: both arguments constants
result = (-2147483647 << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 218: LHS constant
y = 1;
result = (-2147483647 << y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 219: RHS constant
x = -2147483647;
result = (x << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 220: both arguments variables
x = -2147483648;
y = 1;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 221: both arguments constants
result = (-2147483648 << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 222: LHS constant
y = 1;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 223: RHS constant
x = -2147483648;
result = (x << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 224: both arguments variables
x = -2147483649;
y = 1;
result = (x << y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 225: both arguments constants
result = (-2147483649 << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 226: LHS constant
y = 1;
result = (-2147483649 << y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 227: RHS constant
x = -2147483649;
result = (x << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 228: both arguments variables
x = -2147483650;
y = 1;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 229: both arguments constants
result = (-2147483650 << 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 230: LHS constant
y = 1;
result = (-2147483650 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 231: RHS constant
x = -2147483650;
result = (x << 1)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 232: both arguments variables
x = 4294967295;
y = 1;
result = (x << y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 233: both arguments constants
result = (4294967295 << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 234: LHS constant
y = 1;
result = (4294967295 << y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 235: RHS constant
x = 4294967295;
result = (x << 1)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 236: both arguments variables
x = 4294967296;
y = 1;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 237: both arguments constants
result = (4294967296 << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 238: LHS constant
y = 1;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 239: RHS constant
x = 4294967296;
result = (x << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 240: both arguments variables
x = -4294967295;
y = 1;
result = (x << y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 241: both arguments constants
result = (-4294967295 << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 242: LHS constant
y = 1;
result = (-4294967295 << y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 243: RHS constant
x = -4294967295;
result = (x << 1)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 244: both arguments variables
x = -4294967296;
y = 1;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 245: both arguments constants
result = (-4294967296 << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 246: LHS constant
y = 1;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 247: RHS constant
x = -4294967296;
result = (x << 1)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 248: both arguments variables
x = 0;
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 249: both arguments constants
result = (0 << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 250: LHS constant
y = 2;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 251: RHS constant
x = 0;
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 252: both arguments variables
x = 1;
y = 2;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 253: both arguments constants
result = (1 << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 254: LHS constant
y = 2;
result = (1 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 255: RHS constant
x = 1;
result = (x << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 256: both arguments variables
x = -1;
y = 2;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 257: both arguments constants
result = (-1 << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 258: LHS constant
y = 2;
result = (-1 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 259: RHS constant
x = -1;
result = (x << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 260: both arguments variables
x = 2;
y = 2;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 261: both arguments constants
result = (2 << 2)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 262: LHS constant
y = 2;
result = (2 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 263: RHS constant
x = 2;
result = (x << 2)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 264: both arguments variables
x = -2;
y = 2;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 265: both arguments constants
result = (-2 << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 266: LHS constant
y = 2;
result = (-2 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 267: RHS constant
x = -2;
result = (x << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 268: both arguments variables
x = 3;
y = 2;
result = (x << y);
check = 12;
if(result != check) { fail(test, check, result); } ++test; 

// Test 269: both arguments constants
result = (3 << 2)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 270: LHS constant
y = 2;
result = (3 << y)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 271: RHS constant
x = 3;
result = (x << 2)
check = 12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 272: both arguments variables
x = -3;
y = 2;
result = (x << y);
check = -12;
if(result != check) { fail(test, check, result); } ++test; 

// Test 273: both arguments constants
result = (-3 << 2)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 274: LHS constant
y = 2;
result = (-3 << y)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 275: RHS constant
x = -3;
result = (x << 2)
check = -12
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 276: both arguments variables
x = 4;
y = 2;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 277: both arguments constants
result = (4 << 2)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 278: LHS constant
y = 2;
result = (4 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 279: RHS constant
x = 4;
result = (x << 2)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 280: both arguments variables
x = -4;
y = 2;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 281: both arguments constants
result = (-4 << 2)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 282: LHS constant
y = 2;
result = (-4 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 283: RHS constant
x = -4;
result = (x << 2)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 284: both arguments variables
x = 8;
y = 2;
result = (x << y);
check = 32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 285: both arguments constants
result = (8 << 2)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 286: LHS constant
y = 2;
result = (8 << y)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 287: RHS constant
x = 8;
result = (x << 2)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 288: both arguments variables
x = -8;
y = 2;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 289: both arguments constants
result = (-8 << 2)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 290: LHS constant
y = 2;
result = (-8 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 291: RHS constant
x = -8;
result = (x << 2)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 292: both arguments variables
x = 1073741822;
y = 2;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 293: both arguments constants
result = (1073741822 << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 294: LHS constant
y = 2;
result = (1073741822 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 295: RHS constant
x = 1073741822;
result = (x << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 296: both arguments variables
x = 1073741823;
y = 2;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 297: both arguments constants
result = (1073741823 << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 298: LHS constant
y = 2;
result = (1073741823 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 299: RHS constant
x = 1073741823;
result = (x << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test3() {
var x;
var y;
var result;
var check;
// Test 300: both arguments variables
x = 1073741824;
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 301: both arguments constants
result = (1073741824 << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 302: LHS constant
y = 2;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 303: RHS constant
x = 1073741824;
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 304: both arguments variables
x = 1073741825;
y = 2;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 305: both arguments constants
result = (1073741825 << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 306: LHS constant
y = 2;
result = (1073741825 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 307: RHS constant
x = 1073741825;
result = (x << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 308: both arguments variables
x = -1073741823;
y = 2;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 309: both arguments constants
result = (-1073741823 << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 310: LHS constant
y = 2;
result = (-1073741823 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 311: RHS constant
x = -1073741823;
result = (x << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 312: both arguments variables
x = (-0x3fffffff-1);
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 313: both arguments constants
result = ((-0x3fffffff-1) << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 314: LHS constant
y = 2;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 315: RHS constant
x = (-0x3fffffff-1);
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 316: both arguments variables
x = -1073741825;
y = 2;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 317: both arguments constants
result = (-1073741825 << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 318: LHS constant
y = 2;
result = (-1073741825 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 319: RHS constant
x = -1073741825;
result = (x << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 320: both arguments variables
x = -1073741826;
y = 2;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 321: both arguments constants
result = (-1073741826 << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 322: LHS constant
y = 2;
result = (-1073741826 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 323: RHS constant
x = -1073741826;
result = (x << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 324: both arguments variables
x = 2147483646;
y = 2;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 325: both arguments constants
result = (2147483646 << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 326: LHS constant
y = 2;
result = (2147483646 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 327: RHS constant
x = 2147483646;
result = (x << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 328: both arguments variables
x = 2147483647;
y = 2;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 329: both arguments constants
result = (2147483647 << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 330: LHS constant
y = 2;
result = (2147483647 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 331: RHS constant
x = 2147483647;
result = (x << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 332: both arguments variables
x = 2147483648;
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 333: both arguments constants
result = (2147483648 << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 334: LHS constant
y = 2;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 335: RHS constant
x = 2147483648;
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 336: both arguments variables
x = 2147483649;
y = 2;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 337: both arguments constants
result = (2147483649 << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 338: LHS constant
y = 2;
result = (2147483649 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 339: RHS constant
x = 2147483649;
result = (x << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 340: both arguments variables
x = -2147483647;
y = 2;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 341: both arguments constants
result = (-2147483647 << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 342: LHS constant
y = 2;
result = (-2147483647 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 343: RHS constant
x = -2147483647;
result = (x << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 344: both arguments variables
x = -2147483648;
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 345: both arguments constants
result = (-2147483648 << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 346: LHS constant
y = 2;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 347: RHS constant
x = -2147483648;
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 348: both arguments variables
x = -2147483649;
y = 2;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 349: both arguments constants
result = (-2147483649 << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 350: LHS constant
y = 2;
result = (-2147483649 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 351: RHS constant
x = -2147483649;
result = (x << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 352: both arguments variables
x = -2147483650;
y = 2;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 353: both arguments constants
result = (-2147483650 << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 354: LHS constant
y = 2;
result = (-2147483650 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 355: RHS constant
x = -2147483650;
result = (x << 2)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 356: both arguments variables
x = 4294967295;
y = 2;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 357: both arguments constants
result = (4294967295 << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 358: LHS constant
y = 2;
result = (4294967295 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 359: RHS constant
x = 4294967295;
result = (x << 2)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 360: both arguments variables
x = 4294967296;
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 361: both arguments constants
result = (4294967296 << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 362: LHS constant
y = 2;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 363: RHS constant
x = 4294967296;
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 364: both arguments variables
x = -4294967295;
y = 2;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 365: both arguments constants
result = (-4294967295 << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 366: LHS constant
y = 2;
result = (-4294967295 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 367: RHS constant
x = -4294967295;
result = (x << 2)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 368: both arguments variables
x = -4294967296;
y = 2;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 369: both arguments constants
result = (-4294967296 << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 370: LHS constant
y = 2;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 371: RHS constant
x = -4294967296;
result = (x << 2)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 372: both arguments variables
x = 0;
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 373: both arguments constants
result = (0 << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 374: LHS constant
y = 3;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 375: RHS constant
x = 0;
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 376: both arguments variables
x = 1;
y = 3;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 377: both arguments constants
result = (1 << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 378: LHS constant
y = 3;
result = (1 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 379: RHS constant
x = 1;
result = (x << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 380: both arguments variables
x = -1;
y = 3;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 381: both arguments constants
result = (-1 << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 382: LHS constant
y = 3;
result = (-1 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 383: RHS constant
x = -1;
result = (x << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 384: both arguments variables
x = 2;
y = 3;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 385: both arguments constants
result = (2 << 3)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 386: LHS constant
y = 3;
result = (2 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 387: RHS constant
x = 2;
result = (x << 3)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 388: both arguments variables
x = -2;
y = 3;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 389: both arguments constants
result = (-2 << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 390: LHS constant
y = 3;
result = (-2 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 391: RHS constant
x = -2;
result = (x << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 392: both arguments variables
x = 3;
y = 3;
result = (x << y);
check = 24;
if(result != check) { fail(test, check, result); } ++test; 

// Test 393: both arguments constants
result = (3 << 3)
check = 24
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 394: LHS constant
y = 3;
result = (3 << y)
check = 24
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 395: RHS constant
x = 3;
result = (x << 3)
check = 24
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 396: both arguments variables
x = -3;
y = 3;
result = (x << y);
check = -24;
if(result != check) { fail(test, check, result); } ++test; 

// Test 397: both arguments constants
result = (-3 << 3)
check = -24
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 398: LHS constant
y = 3;
result = (-3 << y)
check = -24
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 399: RHS constant
x = -3;
result = (x << 3)
check = -24
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test4() {
var x;
var y;
var result;
var check;
// Test 400: both arguments variables
x = 4;
y = 3;
result = (x << y);
check = 32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 401: both arguments constants
result = (4 << 3)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 402: LHS constant
y = 3;
result = (4 << y)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 403: RHS constant
x = 4;
result = (x << 3)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 404: both arguments variables
x = -4;
y = 3;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 405: both arguments constants
result = (-4 << 3)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 406: LHS constant
y = 3;
result = (-4 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 407: RHS constant
x = -4;
result = (x << 3)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 408: both arguments variables
x = 8;
y = 3;
result = (x << y);
check = 64;
if(result != check) { fail(test, check, result); } ++test; 

// Test 409: both arguments constants
result = (8 << 3)
check = 64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 410: LHS constant
y = 3;
result = (8 << y)
check = 64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 411: RHS constant
x = 8;
result = (x << 3)
check = 64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 412: both arguments variables
x = -8;
y = 3;
result = (x << y);
check = -64;
if(result != check) { fail(test, check, result); } ++test; 

// Test 413: both arguments constants
result = (-8 << 3)
check = -64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 414: LHS constant
y = 3;
result = (-8 << y)
check = -64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 415: RHS constant
x = -8;
result = (x << 3)
check = -64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 416: both arguments variables
x = 1073741822;
y = 3;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 417: both arguments constants
result = (1073741822 << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 418: LHS constant
y = 3;
result = (1073741822 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 419: RHS constant
x = 1073741822;
result = (x << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 420: both arguments variables
x = 1073741823;
y = 3;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 421: both arguments constants
result = (1073741823 << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 422: LHS constant
y = 3;
result = (1073741823 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 423: RHS constant
x = 1073741823;
result = (x << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 424: both arguments variables
x = 1073741824;
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 425: both arguments constants
result = (1073741824 << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 426: LHS constant
y = 3;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 427: RHS constant
x = 1073741824;
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 428: both arguments variables
x = 1073741825;
y = 3;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 429: both arguments constants
result = (1073741825 << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 430: LHS constant
y = 3;
result = (1073741825 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 431: RHS constant
x = 1073741825;
result = (x << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 432: both arguments variables
x = -1073741823;
y = 3;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 433: both arguments constants
result = (-1073741823 << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 434: LHS constant
y = 3;
result = (-1073741823 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 435: RHS constant
x = -1073741823;
result = (x << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 436: both arguments variables
x = (-0x3fffffff-1);
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 437: both arguments constants
result = ((-0x3fffffff-1) << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 438: LHS constant
y = 3;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 439: RHS constant
x = (-0x3fffffff-1);
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 440: both arguments variables
x = -1073741825;
y = 3;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 441: both arguments constants
result = (-1073741825 << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 442: LHS constant
y = 3;
result = (-1073741825 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 443: RHS constant
x = -1073741825;
result = (x << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 444: both arguments variables
x = -1073741826;
y = 3;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 445: both arguments constants
result = (-1073741826 << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 446: LHS constant
y = 3;
result = (-1073741826 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 447: RHS constant
x = -1073741826;
result = (x << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 448: both arguments variables
x = 2147483646;
y = 3;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 449: both arguments constants
result = (2147483646 << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 450: LHS constant
y = 3;
result = (2147483646 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 451: RHS constant
x = 2147483646;
result = (x << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 452: both arguments variables
x = 2147483647;
y = 3;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 453: both arguments constants
result = (2147483647 << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 454: LHS constant
y = 3;
result = (2147483647 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 455: RHS constant
x = 2147483647;
result = (x << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 456: both arguments variables
x = 2147483648;
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 457: both arguments constants
result = (2147483648 << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 458: LHS constant
y = 3;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 459: RHS constant
x = 2147483648;
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 460: both arguments variables
x = 2147483649;
y = 3;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 461: both arguments constants
result = (2147483649 << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 462: LHS constant
y = 3;
result = (2147483649 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 463: RHS constant
x = 2147483649;
result = (x << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 464: both arguments variables
x = -2147483647;
y = 3;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 465: both arguments constants
result = (-2147483647 << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 466: LHS constant
y = 3;
result = (-2147483647 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 467: RHS constant
x = -2147483647;
result = (x << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 468: both arguments variables
x = -2147483648;
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 469: both arguments constants
result = (-2147483648 << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 470: LHS constant
y = 3;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 471: RHS constant
x = -2147483648;
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 472: both arguments variables
x = -2147483649;
y = 3;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 473: both arguments constants
result = (-2147483649 << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 474: LHS constant
y = 3;
result = (-2147483649 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 475: RHS constant
x = -2147483649;
result = (x << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 476: both arguments variables
x = -2147483650;
y = 3;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 477: both arguments constants
result = (-2147483650 << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 478: LHS constant
y = 3;
result = (-2147483650 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 479: RHS constant
x = -2147483650;
result = (x << 3)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 480: both arguments variables
x = 4294967295;
y = 3;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 481: both arguments constants
result = (4294967295 << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 482: LHS constant
y = 3;
result = (4294967295 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 483: RHS constant
x = 4294967295;
result = (x << 3)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 484: both arguments variables
x = 4294967296;
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 485: both arguments constants
result = (4294967296 << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 486: LHS constant
y = 3;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 487: RHS constant
x = 4294967296;
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 488: both arguments variables
x = -4294967295;
y = 3;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 489: both arguments constants
result = (-4294967295 << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 490: LHS constant
y = 3;
result = (-4294967295 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 491: RHS constant
x = -4294967295;
result = (x << 3)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 492: both arguments variables
x = -4294967296;
y = 3;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 493: both arguments constants
result = (-4294967296 << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 494: LHS constant
y = 3;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 495: RHS constant
x = -4294967296;
result = (x << 3)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 496: both arguments variables
x = 0;
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 497: both arguments constants
result = (0 << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 498: LHS constant
y = 4;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 499: RHS constant
x = 0;
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test5() {
var x;
var y;
var result;
var check;
// Test 500: both arguments variables
x = 1;
y = 4;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 501: both arguments constants
result = (1 << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 502: LHS constant
y = 4;
result = (1 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 503: RHS constant
x = 1;
result = (x << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 504: both arguments variables
x = -1;
y = 4;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 505: both arguments constants
result = (-1 << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 506: LHS constant
y = 4;
result = (-1 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 507: RHS constant
x = -1;
result = (x << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 508: both arguments variables
x = 2;
y = 4;
result = (x << y);
check = 32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 509: both arguments constants
result = (2 << 4)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 510: LHS constant
y = 4;
result = (2 << y)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 511: RHS constant
x = 2;
result = (x << 4)
check = 32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 512: both arguments variables
x = -2;
y = 4;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 513: both arguments constants
result = (-2 << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 514: LHS constant
y = 4;
result = (-2 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 515: RHS constant
x = -2;
result = (x << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 516: both arguments variables
x = 3;
y = 4;
result = (x << y);
check = 48;
if(result != check) { fail(test, check, result); } ++test; 

// Test 517: both arguments constants
result = (3 << 4)
check = 48
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 518: LHS constant
y = 4;
result = (3 << y)
check = 48
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 519: RHS constant
x = 3;
result = (x << 4)
check = 48
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 520: both arguments variables
x = -3;
y = 4;
result = (x << y);
check = -48;
if(result != check) { fail(test, check, result); } ++test; 

// Test 521: both arguments constants
result = (-3 << 4)
check = -48
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 522: LHS constant
y = 4;
result = (-3 << y)
check = -48
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 523: RHS constant
x = -3;
result = (x << 4)
check = -48
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 524: both arguments variables
x = 4;
y = 4;
result = (x << y);
check = 64;
if(result != check) { fail(test, check, result); } ++test; 

// Test 525: both arguments constants
result = (4 << 4)
check = 64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 526: LHS constant
y = 4;
result = (4 << y)
check = 64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 527: RHS constant
x = 4;
result = (x << 4)
check = 64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 528: both arguments variables
x = -4;
y = 4;
result = (x << y);
check = -64;
if(result != check) { fail(test, check, result); } ++test; 

// Test 529: both arguments constants
result = (-4 << 4)
check = -64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 530: LHS constant
y = 4;
result = (-4 << y)
check = -64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 531: RHS constant
x = -4;
result = (x << 4)
check = -64
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 532: both arguments variables
x = 8;
y = 4;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 533: both arguments constants
result = (8 << 4)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 534: LHS constant
y = 4;
result = (8 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 535: RHS constant
x = 8;
result = (x << 4)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 536: both arguments variables
x = -8;
y = 4;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 537: both arguments constants
result = (-8 << 4)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 538: LHS constant
y = 4;
result = (-8 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 539: RHS constant
x = -8;
result = (x << 4)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 540: both arguments variables
x = 1073741822;
y = 4;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 541: both arguments constants
result = (1073741822 << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 542: LHS constant
y = 4;
result = (1073741822 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 543: RHS constant
x = 1073741822;
result = (x << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 544: both arguments variables
x = 1073741823;
y = 4;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 545: both arguments constants
result = (1073741823 << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 546: LHS constant
y = 4;
result = (1073741823 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 547: RHS constant
x = 1073741823;
result = (x << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 548: both arguments variables
x = 1073741824;
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 549: both arguments constants
result = (1073741824 << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 550: LHS constant
y = 4;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 551: RHS constant
x = 1073741824;
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 552: both arguments variables
x = 1073741825;
y = 4;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 553: both arguments constants
result = (1073741825 << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 554: LHS constant
y = 4;
result = (1073741825 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 555: RHS constant
x = 1073741825;
result = (x << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 556: both arguments variables
x = -1073741823;
y = 4;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 557: both arguments constants
result = (-1073741823 << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 558: LHS constant
y = 4;
result = (-1073741823 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 559: RHS constant
x = -1073741823;
result = (x << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 560: both arguments variables
x = (-0x3fffffff-1);
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 561: both arguments constants
result = ((-0x3fffffff-1) << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 562: LHS constant
y = 4;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 563: RHS constant
x = (-0x3fffffff-1);
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 564: both arguments variables
x = -1073741825;
y = 4;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 565: both arguments constants
result = (-1073741825 << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 566: LHS constant
y = 4;
result = (-1073741825 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 567: RHS constant
x = -1073741825;
result = (x << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 568: both arguments variables
x = -1073741826;
y = 4;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 569: both arguments constants
result = (-1073741826 << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 570: LHS constant
y = 4;
result = (-1073741826 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 571: RHS constant
x = -1073741826;
result = (x << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 572: both arguments variables
x = 2147483646;
y = 4;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 573: both arguments constants
result = (2147483646 << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 574: LHS constant
y = 4;
result = (2147483646 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 575: RHS constant
x = 2147483646;
result = (x << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 576: both arguments variables
x = 2147483647;
y = 4;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 577: both arguments constants
result = (2147483647 << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 578: LHS constant
y = 4;
result = (2147483647 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 579: RHS constant
x = 2147483647;
result = (x << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 580: both arguments variables
x = 2147483648;
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 581: both arguments constants
result = (2147483648 << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 582: LHS constant
y = 4;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 583: RHS constant
x = 2147483648;
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 584: both arguments variables
x = 2147483649;
y = 4;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 585: both arguments constants
result = (2147483649 << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 586: LHS constant
y = 4;
result = (2147483649 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 587: RHS constant
x = 2147483649;
result = (x << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 588: both arguments variables
x = -2147483647;
y = 4;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 589: both arguments constants
result = (-2147483647 << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 590: LHS constant
y = 4;
result = (-2147483647 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 591: RHS constant
x = -2147483647;
result = (x << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 592: both arguments variables
x = -2147483648;
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 593: both arguments constants
result = (-2147483648 << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 594: LHS constant
y = 4;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 595: RHS constant
x = -2147483648;
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 596: both arguments variables
x = -2147483649;
y = 4;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 597: both arguments constants
result = (-2147483649 << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 598: LHS constant
y = 4;
result = (-2147483649 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 599: RHS constant
x = -2147483649;
result = (x << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test6() {
var x;
var y;
var result;
var check;
// Test 600: both arguments variables
x = -2147483650;
y = 4;
result = (x << y);
check = -32;
if(result != check) { fail(test, check, result); } ++test; 

// Test 601: both arguments constants
result = (-2147483650 << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 602: LHS constant
y = 4;
result = (-2147483650 << y)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 603: RHS constant
x = -2147483650;
result = (x << 4)
check = -32
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 604: both arguments variables
x = 4294967295;
y = 4;
result = (x << y);
check = -16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 605: both arguments constants
result = (4294967295 << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 606: LHS constant
y = 4;
result = (4294967295 << y)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 607: RHS constant
x = 4294967295;
result = (x << 4)
check = -16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 608: both arguments variables
x = 4294967296;
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 609: both arguments constants
result = (4294967296 << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 610: LHS constant
y = 4;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 611: RHS constant
x = 4294967296;
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 612: both arguments variables
x = -4294967295;
y = 4;
result = (x << y);
check = 16;
if(result != check) { fail(test, check, result); } ++test; 

// Test 613: both arguments constants
result = (-4294967295 << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 614: LHS constant
y = 4;
result = (-4294967295 << y)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 615: RHS constant
x = -4294967295;
result = (x << 4)
check = 16
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 616: both arguments variables
x = -4294967296;
y = 4;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 617: both arguments constants
result = (-4294967296 << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 618: LHS constant
y = 4;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 619: RHS constant
x = -4294967296;
result = (x << 4)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 620: both arguments variables
x = 0;
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 621: both arguments constants
result = (0 << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 622: LHS constant
y = 7;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 623: RHS constant
x = 0;
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 624: both arguments variables
x = 1;
y = 7;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 625: both arguments constants
result = (1 << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 626: LHS constant
y = 7;
result = (1 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 627: RHS constant
x = 1;
result = (x << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 628: both arguments variables
x = -1;
y = 7;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 629: both arguments constants
result = (-1 << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 630: LHS constant
y = 7;
result = (-1 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 631: RHS constant
x = -1;
result = (x << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 632: both arguments variables
x = 2;
y = 7;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 633: both arguments constants
result = (2 << 7)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 634: LHS constant
y = 7;
result = (2 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 635: RHS constant
x = 2;
result = (x << 7)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 636: both arguments variables
x = -2;
y = 7;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 637: both arguments constants
result = (-2 << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 638: LHS constant
y = 7;
result = (-2 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 639: RHS constant
x = -2;
result = (x << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 640: both arguments variables
x = 3;
y = 7;
result = (x << y);
check = 384;
if(result != check) { fail(test, check, result); } ++test; 

// Test 641: both arguments constants
result = (3 << 7)
check = 384
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 642: LHS constant
y = 7;
result = (3 << y)
check = 384
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 643: RHS constant
x = 3;
result = (x << 7)
check = 384
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 644: both arguments variables
x = -3;
y = 7;
result = (x << y);
check = -384;
if(result != check) { fail(test, check, result); } ++test; 

// Test 645: both arguments constants
result = (-3 << 7)
check = -384
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 646: LHS constant
y = 7;
result = (-3 << y)
check = -384
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 647: RHS constant
x = -3;
result = (x << 7)
check = -384
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 648: both arguments variables
x = 4;
y = 7;
result = (x << y);
check = 512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 649: both arguments constants
result = (4 << 7)
check = 512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 650: LHS constant
y = 7;
result = (4 << y)
check = 512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 651: RHS constant
x = 4;
result = (x << 7)
check = 512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 652: both arguments variables
x = -4;
y = 7;
result = (x << y);
check = -512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 653: both arguments constants
result = (-4 << 7)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 654: LHS constant
y = 7;
result = (-4 << y)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 655: RHS constant
x = -4;
result = (x << 7)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 656: both arguments variables
x = 8;
y = 7;
result = (x << y);
check = 1024;
if(result != check) { fail(test, check, result); } ++test; 

// Test 657: both arguments constants
result = (8 << 7)
check = 1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 658: LHS constant
y = 7;
result = (8 << y)
check = 1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 659: RHS constant
x = 8;
result = (x << 7)
check = 1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 660: both arguments variables
x = -8;
y = 7;
result = (x << y);
check = -1024;
if(result != check) { fail(test, check, result); } ++test; 

// Test 661: both arguments constants
result = (-8 << 7)
check = -1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 662: LHS constant
y = 7;
result = (-8 << y)
check = -1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 663: RHS constant
x = -8;
result = (x << 7)
check = -1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 664: both arguments variables
x = 1073741822;
y = 7;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 665: both arguments constants
result = (1073741822 << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 666: LHS constant
y = 7;
result = (1073741822 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 667: RHS constant
x = 1073741822;
result = (x << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 668: both arguments variables
x = 1073741823;
y = 7;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 669: both arguments constants
result = (1073741823 << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 670: LHS constant
y = 7;
result = (1073741823 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 671: RHS constant
x = 1073741823;
result = (x << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 672: both arguments variables
x = 1073741824;
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 673: both arguments constants
result = (1073741824 << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 674: LHS constant
y = 7;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 675: RHS constant
x = 1073741824;
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 676: both arguments variables
x = 1073741825;
y = 7;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 677: both arguments constants
result = (1073741825 << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 678: LHS constant
y = 7;
result = (1073741825 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 679: RHS constant
x = 1073741825;
result = (x << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 680: both arguments variables
x = -1073741823;
y = 7;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 681: both arguments constants
result = (-1073741823 << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 682: LHS constant
y = 7;
result = (-1073741823 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 683: RHS constant
x = -1073741823;
result = (x << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 684: both arguments variables
x = (-0x3fffffff-1);
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 685: both arguments constants
result = ((-0x3fffffff-1) << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 686: LHS constant
y = 7;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 687: RHS constant
x = (-0x3fffffff-1);
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 688: both arguments variables
x = -1073741825;
y = 7;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 689: both arguments constants
result = (-1073741825 << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 690: LHS constant
y = 7;
result = (-1073741825 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 691: RHS constant
x = -1073741825;
result = (x << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 692: both arguments variables
x = -1073741826;
y = 7;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 693: both arguments constants
result = (-1073741826 << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 694: LHS constant
y = 7;
result = (-1073741826 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 695: RHS constant
x = -1073741826;
result = (x << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 696: both arguments variables
x = 2147483646;
y = 7;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 697: both arguments constants
result = (2147483646 << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 698: LHS constant
y = 7;
result = (2147483646 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 699: RHS constant
x = 2147483646;
result = (x << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test7() {
var x;
var y;
var result;
var check;
// Test 700: both arguments variables
x = 2147483647;
y = 7;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 701: both arguments constants
result = (2147483647 << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 702: LHS constant
y = 7;
result = (2147483647 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 703: RHS constant
x = 2147483647;
result = (x << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 704: both arguments variables
x = 2147483648;
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 705: both arguments constants
result = (2147483648 << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 706: LHS constant
y = 7;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 707: RHS constant
x = 2147483648;
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 708: both arguments variables
x = 2147483649;
y = 7;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 709: both arguments constants
result = (2147483649 << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 710: LHS constant
y = 7;
result = (2147483649 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 711: RHS constant
x = 2147483649;
result = (x << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 712: both arguments variables
x = -2147483647;
y = 7;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 713: both arguments constants
result = (-2147483647 << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 714: LHS constant
y = 7;
result = (-2147483647 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 715: RHS constant
x = -2147483647;
result = (x << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 716: both arguments variables
x = -2147483648;
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 717: both arguments constants
result = (-2147483648 << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 718: LHS constant
y = 7;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 719: RHS constant
x = -2147483648;
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 720: both arguments variables
x = -2147483649;
y = 7;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 721: both arguments constants
result = (-2147483649 << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 722: LHS constant
y = 7;
result = (-2147483649 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 723: RHS constant
x = -2147483649;
result = (x << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 724: both arguments variables
x = -2147483650;
y = 7;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 725: both arguments constants
result = (-2147483650 << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 726: LHS constant
y = 7;
result = (-2147483650 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 727: RHS constant
x = -2147483650;
result = (x << 7)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 728: both arguments variables
x = 4294967295;
y = 7;
result = (x << y);
check = -128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 729: both arguments constants
result = (4294967295 << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 730: LHS constant
y = 7;
result = (4294967295 << y)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 731: RHS constant
x = 4294967295;
result = (x << 7)
check = -128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 732: both arguments variables
x = 4294967296;
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 733: both arguments constants
result = (4294967296 << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 734: LHS constant
y = 7;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 735: RHS constant
x = 4294967296;
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 736: both arguments variables
x = -4294967295;
y = 7;
result = (x << y);
check = 128;
if(result != check) { fail(test, check, result); } ++test; 

// Test 737: both arguments constants
result = (-4294967295 << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 738: LHS constant
y = 7;
result = (-4294967295 << y)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 739: RHS constant
x = -4294967295;
result = (x << 7)
check = 128
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 740: both arguments variables
x = -4294967296;
y = 7;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 741: both arguments constants
result = (-4294967296 << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 742: LHS constant
y = 7;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 743: RHS constant
x = -4294967296;
result = (x << 7)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 744: both arguments variables
x = 0;
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 745: both arguments constants
result = (0 << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 746: LHS constant
y = 8;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 747: RHS constant
x = 0;
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 748: both arguments variables
x = 1;
y = 8;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 749: both arguments constants
result = (1 << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 750: LHS constant
y = 8;
result = (1 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 751: RHS constant
x = 1;
result = (x << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 752: both arguments variables
x = -1;
y = 8;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 753: both arguments constants
result = (-1 << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 754: LHS constant
y = 8;
result = (-1 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 755: RHS constant
x = -1;
result = (x << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 756: both arguments variables
x = 2;
y = 8;
result = (x << y);
check = 512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 757: both arguments constants
result = (2 << 8)
check = 512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 758: LHS constant
y = 8;
result = (2 << y)
check = 512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 759: RHS constant
x = 2;
result = (x << 8)
check = 512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 760: both arguments variables
x = -2;
y = 8;
result = (x << y);
check = -512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 761: both arguments constants
result = (-2 << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 762: LHS constant
y = 8;
result = (-2 << y)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 763: RHS constant
x = -2;
result = (x << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 764: both arguments variables
x = 3;
y = 8;
result = (x << y);
check = 768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 765: both arguments constants
result = (3 << 8)
check = 768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 766: LHS constant
y = 8;
result = (3 << y)
check = 768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 767: RHS constant
x = 3;
result = (x << 8)
check = 768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 768: both arguments variables
x = -3;
y = 8;
result = (x << y);
check = -768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 769: both arguments constants
result = (-3 << 8)
check = -768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 770: LHS constant
y = 8;
result = (-3 << y)
check = -768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 771: RHS constant
x = -3;
result = (x << 8)
check = -768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 772: both arguments variables
x = 4;
y = 8;
result = (x << y);
check = 1024;
if(result != check) { fail(test, check, result); } ++test; 

// Test 773: both arguments constants
result = (4 << 8)
check = 1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 774: LHS constant
y = 8;
result = (4 << y)
check = 1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 775: RHS constant
x = 4;
result = (x << 8)
check = 1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 776: both arguments variables
x = -4;
y = 8;
result = (x << y);
check = -1024;
if(result != check) { fail(test, check, result); } ++test; 

// Test 777: both arguments constants
result = (-4 << 8)
check = -1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 778: LHS constant
y = 8;
result = (-4 << y)
check = -1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 779: RHS constant
x = -4;
result = (x << 8)
check = -1024
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 780: both arguments variables
x = 8;
y = 8;
result = (x << y);
check = 2048;
if(result != check) { fail(test, check, result); } ++test; 

// Test 781: both arguments constants
result = (8 << 8)
check = 2048
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 782: LHS constant
y = 8;
result = (8 << y)
check = 2048
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 783: RHS constant
x = 8;
result = (x << 8)
check = 2048
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 784: both arguments variables
x = -8;
y = 8;
result = (x << y);
check = -2048;
if(result != check) { fail(test, check, result); } ++test; 

// Test 785: both arguments constants
result = (-8 << 8)
check = -2048
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 786: LHS constant
y = 8;
result = (-8 << y)
check = -2048
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 787: RHS constant
x = -8;
result = (x << 8)
check = -2048
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 788: both arguments variables
x = 1073741822;
y = 8;
result = (x << y);
check = -512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 789: both arguments constants
result = (1073741822 << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 790: LHS constant
y = 8;
result = (1073741822 << y)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 791: RHS constant
x = 1073741822;
result = (x << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 792: both arguments variables
x = 1073741823;
y = 8;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 793: both arguments constants
result = (1073741823 << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 794: LHS constant
y = 8;
result = (1073741823 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 795: RHS constant
x = 1073741823;
result = (x << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 796: both arguments variables
x = 1073741824;
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 797: both arguments constants
result = (1073741824 << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 798: LHS constant
y = 8;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 799: RHS constant
x = 1073741824;
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test8() {
var x;
var y;
var result;
var check;
// Test 800: both arguments variables
x = 1073741825;
y = 8;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 801: both arguments constants
result = (1073741825 << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 802: LHS constant
y = 8;
result = (1073741825 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 803: RHS constant
x = 1073741825;
result = (x << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 804: both arguments variables
x = -1073741823;
y = 8;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 805: both arguments constants
result = (-1073741823 << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 806: LHS constant
y = 8;
result = (-1073741823 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 807: RHS constant
x = -1073741823;
result = (x << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 808: both arguments variables
x = (-0x3fffffff-1);
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 809: both arguments constants
result = ((-0x3fffffff-1) << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 810: LHS constant
y = 8;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 811: RHS constant
x = (-0x3fffffff-1);
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 812: both arguments variables
x = -1073741825;
y = 8;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 813: both arguments constants
result = (-1073741825 << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 814: LHS constant
y = 8;
result = (-1073741825 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 815: RHS constant
x = -1073741825;
result = (x << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 816: both arguments variables
x = -1073741826;
y = 8;
result = (x << y);
check = -512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 817: both arguments constants
result = (-1073741826 << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 818: LHS constant
y = 8;
result = (-1073741826 << y)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 819: RHS constant
x = -1073741826;
result = (x << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 820: both arguments variables
x = 2147483646;
y = 8;
result = (x << y);
check = -512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 821: both arguments constants
result = (2147483646 << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 822: LHS constant
y = 8;
result = (2147483646 << y)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 823: RHS constant
x = 2147483646;
result = (x << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 824: both arguments variables
x = 2147483647;
y = 8;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 825: both arguments constants
result = (2147483647 << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 826: LHS constant
y = 8;
result = (2147483647 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 827: RHS constant
x = 2147483647;
result = (x << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 828: both arguments variables
x = 2147483648;
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 829: both arguments constants
result = (2147483648 << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 830: LHS constant
y = 8;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 831: RHS constant
x = 2147483648;
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 832: both arguments variables
x = 2147483649;
y = 8;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 833: both arguments constants
result = (2147483649 << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 834: LHS constant
y = 8;
result = (2147483649 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 835: RHS constant
x = 2147483649;
result = (x << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 836: both arguments variables
x = -2147483647;
y = 8;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 837: both arguments constants
result = (-2147483647 << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 838: LHS constant
y = 8;
result = (-2147483647 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 839: RHS constant
x = -2147483647;
result = (x << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 840: both arguments variables
x = -2147483648;
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 841: both arguments constants
result = (-2147483648 << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 842: LHS constant
y = 8;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 843: RHS constant
x = -2147483648;
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 844: both arguments variables
x = -2147483649;
y = 8;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 845: both arguments constants
result = (-2147483649 << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 846: LHS constant
y = 8;
result = (-2147483649 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 847: RHS constant
x = -2147483649;
result = (x << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 848: both arguments variables
x = -2147483650;
y = 8;
result = (x << y);
check = -512;
if(result != check) { fail(test, check, result); } ++test; 

// Test 849: both arguments constants
result = (-2147483650 << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 850: LHS constant
y = 8;
result = (-2147483650 << y)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 851: RHS constant
x = -2147483650;
result = (x << 8)
check = -512
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 852: both arguments variables
x = 4294967295;
y = 8;
result = (x << y);
check = -256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 853: both arguments constants
result = (4294967295 << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 854: LHS constant
y = 8;
result = (4294967295 << y)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 855: RHS constant
x = 4294967295;
result = (x << 8)
check = -256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 856: both arguments variables
x = 4294967296;
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 857: both arguments constants
result = (4294967296 << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 858: LHS constant
y = 8;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 859: RHS constant
x = 4294967296;
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 860: both arguments variables
x = -4294967295;
y = 8;
result = (x << y);
check = 256;
if(result != check) { fail(test, check, result); } ++test; 

// Test 861: both arguments constants
result = (-4294967295 << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 862: LHS constant
y = 8;
result = (-4294967295 << y)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 863: RHS constant
x = -4294967295;
result = (x << 8)
check = 256
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 864: both arguments variables
x = -4294967296;
y = 8;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 865: both arguments constants
result = (-4294967296 << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 866: LHS constant
y = 8;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 867: RHS constant
x = -4294967296;
result = (x << 8)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 868: both arguments variables
x = 0;
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 869: both arguments constants
result = (0 << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 870: LHS constant
y = 15;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 871: RHS constant
x = 0;
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 872: both arguments variables
x = 1;
y = 15;
result = (x << y);
check = 32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 873: both arguments constants
result = (1 << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 874: LHS constant
y = 15;
result = (1 << y)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 875: RHS constant
x = 1;
result = (x << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 876: both arguments variables
x = -1;
y = 15;
result = (x << y);
check = -32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 877: both arguments constants
result = (-1 << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 878: LHS constant
y = 15;
result = (-1 << y)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 879: RHS constant
x = -1;
result = (x << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 880: both arguments variables
x = 2;
y = 15;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 881: both arguments constants
result = (2 << 15)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 882: LHS constant
y = 15;
result = (2 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 883: RHS constant
x = 2;
result = (x << 15)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 884: both arguments variables
x = -2;
y = 15;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 885: both arguments constants
result = (-2 << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 886: LHS constant
y = 15;
result = (-2 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 887: RHS constant
x = -2;
result = (x << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 888: both arguments variables
x = 3;
y = 15;
result = (x << y);
check = 98304;
if(result != check) { fail(test, check, result); } ++test; 

// Test 889: both arguments constants
result = (3 << 15)
check = 98304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 890: LHS constant
y = 15;
result = (3 << y)
check = 98304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 891: RHS constant
x = 3;
result = (x << 15)
check = 98304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 892: both arguments variables
x = -3;
y = 15;
result = (x << y);
check = -98304;
if(result != check) { fail(test, check, result); } ++test; 

// Test 893: both arguments constants
result = (-3 << 15)
check = -98304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 894: LHS constant
y = 15;
result = (-3 << y)
check = -98304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 895: RHS constant
x = -3;
result = (x << 15)
check = -98304
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 896: both arguments variables
x = 4;
y = 15;
result = (x << y);
check = 131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 897: both arguments constants
result = (4 << 15)
check = 131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 898: LHS constant
y = 15;
result = (4 << y)
check = 131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 899: RHS constant
x = 4;
result = (x << 15)
check = 131072
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test9() {
var x;
var y;
var result;
var check;
// Test 900: both arguments variables
x = -4;
y = 15;
result = (x << y);
check = -131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 901: both arguments constants
result = (-4 << 15)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 902: LHS constant
y = 15;
result = (-4 << y)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 903: RHS constant
x = -4;
result = (x << 15)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 904: both arguments variables
x = 8;
y = 15;
result = (x << y);
check = 262144;
if(result != check) { fail(test, check, result); } ++test; 

// Test 905: both arguments constants
result = (8 << 15)
check = 262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 906: LHS constant
y = 15;
result = (8 << y)
check = 262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 907: RHS constant
x = 8;
result = (x << 15)
check = 262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 908: both arguments variables
x = -8;
y = 15;
result = (x << y);
check = -262144;
if(result != check) { fail(test, check, result); } ++test; 

// Test 909: both arguments constants
result = (-8 << 15)
check = -262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 910: LHS constant
y = 15;
result = (-8 << y)
check = -262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 911: RHS constant
x = -8;
result = (x << 15)
check = -262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 912: both arguments variables
x = 1073741822;
y = 15;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 913: both arguments constants
result = (1073741822 << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 914: LHS constant
y = 15;
result = (1073741822 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 915: RHS constant
x = 1073741822;
result = (x << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 916: both arguments variables
x = 1073741823;
y = 15;
result = (x << y);
check = -32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 917: both arguments constants
result = (1073741823 << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 918: LHS constant
y = 15;
result = (1073741823 << y)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 919: RHS constant
x = 1073741823;
result = (x << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 920: both arguments variables
x = 1073741824;
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 921: both arguments constants
result = (1073741824 << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 922: LHS constant
y = 15;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 923: RHS constant
x = 1073741824;
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 924: both arguments variables
x = 1073741825;
y = 15;
result = (x << y);
check = 32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 925: both arguments constants
result = (1073741825 << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 926: LHS constant
y = 15;
result = (1073741825 << y)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 927: RHS constant
x = 1073741825;
result = (x << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 928: both arguments variables
x = -1073741823;
y = 15;
result = (x << y);
check = 32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 929: both arguments constants
result = (-1073741823 << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 930: LHS constant
y = 15;
result = (-1073741823 << y)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 931: RHS constant
x = -1073741823;
result = (x << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 932: both arguments variables
x = (-0x3fffffff-1);
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 933: both arguments constants
result = ((-0x3fffffff-1) << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 934: LHS constant
y = 15;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 935: RHS constant
x = (-0x3fffffff-1);
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 936: both arguments variables
x = -1073741825;
y = 15;
result = (x << y);
check = -32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 937: both arguments constants
result = (-1073741825 << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 938: LHS constant
y = 15;
result = (-1073741825 << y)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 939: RHS constant
x = -1073741825;
result = (x << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 940: both arguments variables
x = -1073741826;
y = 15;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 941: both arguments constants
result = (-1073741826 << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 942: LHS constant
y = 15;
result = (-1073741826 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 943: RHS constant
x = -1073741826;
result = (x << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 944: both arguments variables
x = 2147483646;
y = 15;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 945: both arguments constants
result = (2147483646 << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 946: LHS constant
y = 15;
result = (2147483646 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 947: RHS constant
x = 2147483646;
result = (x << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 948: both arguments variables
x = 2147483647;
y = 15;
result = (x << y);
check = -32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 949: both arguments constants
result = (2147483647 << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 950: LHS constant
y = 15;
result = (2147483647 << y)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 951: RHS constant
x = 2147483647;
result = (x << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 952: both arguments variables
x = 2147483648;
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 953: both arguments constants
result = (2147483648 << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 954: LHS constant
y = 15;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 955: RHS constant
x = 2147483648;
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 956: both arguments variables
x = 2147483649;
y = 15;
result = (x << y);
check = 32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 957: both arguments constants
result = (2147483649 << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 958: LHS constant
y = 15;
result = (2147483649 << y)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 959: RHS constant
x = 2147483649;
result = (x << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 960: both arguments variables
x = -2147483647;
y = 15;
result = (x << y);
check = 32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 961: both arguments constants
result = (-2147483647 << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 962: LHS constant
y = 15;
result = (-2147483647 << y)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 963: RHS constant
x = -2147483647;
result = (x << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 964: both arguments variables
x = -2147483648;
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 965: both arguments constants
result = (-2147483648 << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 966: LHS constant
y = 15;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 967: RHS constant
x = -2147483648;
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 968: both arguments variables
x = -2147483649;
y = 15;
result = (x << y);
check = -32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 969: both arguments constants
result = (-2147483649 << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 970: LHS constant
y = 15;
result = (-2147483649 << y)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 971: RHS constant
x = -2147483649;
result = (x << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 972: both arguments variables
x = -2147483650;
y = 15;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 973: both arguments constants
result = (-2147483650 << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 974: LHS constant
y = 15;
result = (-2147483650 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 975: RHS constant
x = -2147483650;
result = (x << 15)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 976: both arguments variables
x = 4294967295;
y = 15;
result = (x << y);
check = -32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 977: both arguments constants
result = (4294967295 << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 978: LHS constant
y = 15;
result = (4294967295 << y)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 979: RHS constant
x = 4294967295;
result = (x << 15)
check = -32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 980: both arguments variables
x = 4294967296;
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 981: both arguments constants
result = (4294967296 << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 982: LHS constant
y = 15;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 983: RHS constant
x = 4294967296;
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 984: both arguments variables
x = -4294967295;
y = 15;
result = (x << y);
check = 32768;
if(result != check) { fail(test, check, result); } ++test; 

// Test 985: both arguments constants
result = (-4294967295 << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 986: LHS constant
y = 15;
result = (-4294967295 << y)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 987: RHS constant
x = -4294967295;
result = (x << 15)
check = 32768
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 988: both arguments variables
x = -4294967296;
y = 15;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 989: both arguments constants
result = (-4294967296 << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 990: LHS constant
y = 15;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 991: RHS constant
x = -4294967296;
result = (x << 15)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 992: both arguments variables
x = 0;
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 993: both arguments constants
result = (0 << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 994: LHS constant
y = 16;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 995: RHS constant
x = 0;
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 996: both arguments variables
x = 1;
y = 16;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 997: both arguments constants
result = (1 << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 998: LHS constant
y = 16;
result = (1 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 999: RHS constant
x = 1;
result = (x << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test10() {
var x;
var y;
var result;
var check;
// Test 1000: both arguments variables
x = -1;
y = 16;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1001: both arguments constants
result = (-1 << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1002: LHS constant
y = 16;
result = (-1 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1003: RHS constant
x = -1;
result = (x << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1004: both arguments variables
x = 2;
y = 16;
result = (x << y);
check = 131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1005: both arguments constants
result = (2 << 16)
check = 131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1006: LHS constant
y = 16;
result = (2 << y)
check = 131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1007: RHS constant
x = 2;
result = (x << 16)
check = 131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1008: both arguments variables
x = -2;
y = 16;
result = (x << y);
check = -131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1009: both arguments constants
result = (-2 << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1010: LHS constant
y = 16;
result = (-2 << y)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1011: RHS constant
x = -2;
result = (x << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1012: both arguments variables
x = 3;
y = 16;
result = (x << y);
check = 196608;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1013: both arguments constants
result = (3 << 16)
check = 196608
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1014: LHS constant
y = 16;
result = (3 << y)
check = 196608
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1015: RHS constant
x = 3;
result = (x << 16)
check = 196608
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1016: both arguments variables
x = -3;
y = 16;
result = (x << y);
check = -196608;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1017: both arguments constants
result = (-3 << 16)
check = -196608
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1018: LHS constant
y = 16;
result = (-3 << y)
check = -196608
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1019: RHS constant
x = -3;
result = (x << 16)
check = -196608
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1020: both arguments variables
x = 4;
y = 16;
result = (x << y);
check = 262144;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1021: both arguments constants
result = (4 << 16)
check = 262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1022: LHS constant
y = 16;
result = (4 << y)
check = 262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1023: RHS constant
x = 4;
result = (x << 16)
check = 262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1024: both arguments variables
x = -4;
y = 16;
result = (x << y);
check = -262144;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1025: both arguments constants
result = (-4 << 16)
check = -262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1026: LHS constant
y = 16;
result = (-4 << y)
check = -262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1027: RHS constant
x = -4;
result = (x << 16)
check = -262144
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1028: both arguments variables
x = 8;
y = 16;
result = (x << y);
check = 524288;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1029: both arguments constants
result = (8 << 16)
check = 524288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1030: LHS constant
y = 16;
result = (8 << y)
check = 524288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1031: RHS constant
x = 8;
result = (x << 16)
check = 524288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1032: both arguments variables
x = -8;
y = 16;
result = (x << y);
check = -524288;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1033: both arguments constants
result = (-8 << 16)
check = -524288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1034: LHS constant
y = 16;
result = (-8 << y)
check = -524288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1035: RHS constant
x = -8;
result = (x << 16)
check = -524288
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1036: both arguments variables
x = 1073741822;
y = 16;
result = (x << y);
check = -131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1037: both arguments constants
result = (1073741822 << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1038: LHS constant
y = 16;
result = (1073741822 << y)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1039: RHS constant
x = 1073741822;
result = (x << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1040: both arguments variables
x = 1073741823;
y = 16;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1041: both arguments constants
result = (1073741823 << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1042: LHS constant
y = 16;
result = (1073741823 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1043: RHS constant
x = 1073741823;
result = (x << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1044: both arguments variables
x = 1073741824;
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1045: both arguments constants
result = (1073741824 << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1046: LHS constant
y = 16;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1047: RHS constant
x = 1073741824;
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1048: both arguments variables
x = 1073741825;
y = 16;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1049: both arguments constants
result = (1073741825 << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1050: LHS constant
y = 16;
result = (1073741825 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1051: RHS constant
x = 1073741825;
result = (x << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1052: both arguments variables
x = -1073741823;
y = 16;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1053: both arguments constants
result = (-1073741823 << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1054: LHS constant
y = 16;
result = (-1073741823 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1055: RHS constant
x = -1073741823;
result = (x << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1056: both arguments variables
x = (-0x3fffffff-1);
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1057: both arguments constants
result = ((-0x3fffffff-1) << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1058: LHS constant
y = 16;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1059: RHS constant
x = (-0x3fffffff-1);
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1060: both arguments variables
x = -1073741825;
y = 16;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1061: both arguments constants
result = (-1073741825 << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1062: LHS constant
y = 16;
result = (-1073741825 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1063: RHS constant
x = -1073741825;
result = (x << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1064: both arguments variables
x = -1073741826;
y = 16;
result = (x << y);
check = -131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1065: both arguments constants
result = (-1073741826 << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1066: LHS constant
y = 16;
result = (-1073741826 << y)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1067: RHS constant
x = -1073741826;
result = (x << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1068: both arguments variables
x = 2147483646;
y = 16;
result = (x << y);
check = -131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1069: both arguments constants
result = (2147483646 << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1070: LHS constant
y = 16;
result = (2147483646 << y)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1071: RHS constant
x = 2147483646;
result = (x << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1072: both arguments variables
x = 2147483647;
y = 16;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1073: both arguments constants
result = (2147483647 << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1074: LHS constant
y = 16;
result = (2147483647 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1075: RHS constant
x = 2147483647;
result = (x << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1076: both arguments variables
x = 2147483648;
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1077: both arguments constants
result = (2147483648 << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1078: LHS constant
y = 16;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1079: RHS constant
x = 2147483648;
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1080: both arguments variables
x = 2147483649;
y = 16;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1081: both arguments constants
result = (2147483649 << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1082: LHS constant
y = 16;
result = (2147483649 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1083: RHS constant
x = 2147483649;
result = (x << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1084: both arguments variables
x = -2147483647;
y = 16;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1085: both arguments constants
result = (-2147483647 << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1086: LHS constant
y = 16;
result = (-2147483647 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1087: RHS constant
x = -2147483647;
result = (x << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1088: both arguments variables
x = -2147483648;
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1089: both arguments constants
result = (-2147483648 << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1090: LHS constant
y = 16;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1091: RHS constant
x = -2147483648;
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1092: both arguments variables
x = -2147483649;
y = 16;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1093: both arguments constants
result = (-2147483649 << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1094: LHS constant
y = 16;
result = (-2147483649 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1095: RHS constant
x = -2147483649;
result = (x << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1096: both arguments variables
x = -2147483650;
y = 16;
result = (x << y);
check = -131072;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1097: both arguments constants
result = (-2147483650 << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1098: LHS constant
y = 16;
result = (-2147483650 << y)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1099: RHS constant
x = -2147483650;
result = (x << 16)
check = -131072
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test11() {
var x;
var y;
var result;
var check;
// Test 1100: both arguments variables
x = 4294967295;
y = 16;
result = (x << y);
check = -65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1101: both arguments constants
result = (4294967295 << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1102: LHS constant
y = 16;
result = (4294967295 << y)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1103: RHS constant
x = 4294967295;
result = (x << 16)
check = -65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1104: both arguments variables
x = 4294967296;
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1105: both arguments constants
result = (4294967296 << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1106: LHS constant
y = 16;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1107: RHS constant
x = 4294967296;
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1108: both arguments variables
x = -4294967295;
y = 16;
result = (x << y);
check = 65536;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1109: both arguments constants
result = (-4294967295 << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1110: LHS constant
y = 16;
result = (-4294967295 << y)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1111: RHS constant
x = -4294967295;
result = (x << 16)
check = 65536
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1112: both arguments variables
x = -4294967296;
y = 16;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1113: both arguments constants
result = (-4294967296 << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1114: LHS constant
y = 16;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1115: RHS constant
x = -4294967296;
result = (x << 16)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1116: both arguments variables
x = 0;
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1117: both arguments constants
result = (0 << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1118: LHS constant
y = 27;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1119: RHS constant
x = 0;
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1120: both arguments variables
x = 1;
y = 27;
result = (x << y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1121: both arguments constants
result = (1 << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1122: LHS constant
y = 27;
result = (1 << y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1123: RHS constant
x = 1;
result = (x << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1124: both arguments variables
x = -1;
y = 27;
result = (x << y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1125: both arguments constants
result = (-1 << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1126: LHS constant
y = 27;
result = (-1 << y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1127: RHS constant
x = -1;
result = (x << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1128: both arguments variables
x = 2;
y = 27;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1129: both arguments constants
result = (2 << 27)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1130: LHS constant
y = 27;
result = (2 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1131: RHS constant
x = 2;
result = (x << 27)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1132: both arguments variables
x = -2;
y = 27;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1133: both arguments constants
result = (-2 << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1134: LHS constant
y = 27;
result = (-2 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1135: RHS constant
x = -2;
result = (x << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1136: both arguments variables
x = 3;
y = 27;
result = (x << y);
check = 402653184;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1137: both arguments constants
result = (3 << 27)
check = 402653184
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1138: LHS constant
y = 27;
result = (3 << y)
check = 402653184
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1139: RHS constant
x = 3;
result = (x << 27)
check = 402653184
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1140: both arguments variables
x = -3;
y = 27;
result = (x << y);
check = -402653184;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1141: both arguments constants
result = (-3 << 27)
check = -402653184
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1142: LHS constant
y = 27;
result = (-3 << y)
check = -402653184
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1143: RHS constant
x = -3;
result = (x << 27)
check = -402653184
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1144: both arguments variables
x = 4;
y = 27;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1145: both arguments constants
result = (4 << 27)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1146: LHS constant
y = 27;
result = (4 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1147: RHS constant
x = 4;
result = (x << 27)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1148: both arguments variables
x = -4;
y = 27;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1149: both arguments constants
result = (-4 << 27)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1150: LHS constant
y = 27;
result = (-4 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1151: RHS constant
x = -4;
result = (x << 27)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1152: both arguments variables
x = 8;
y = 27;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1153: both arguments constants
result = (8 << 27)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1154: LHS constant
y = 27;
result = (8 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1155: RHS constant
x = 8;
result = (x << 27)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1156: both arguments variables
x = -8;
y = 27;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1157: both arguments constants
result = (-8 << 27)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1158: LHS constant
y = 27;
result = (-8 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1159: RHS constant
x = -8;
result = (x << 27)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1160: both arguments variables
x = 1073741822;
y = 27;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1161: both arguments constants
result = (1073741822 << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1162: LHS constant
y = 27;
result = (1073741822 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1163: RHS constant
x = 1073741822;
result = (x << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1164: both arguments variables
x = 1073741823;
y = 27;
result = (x << y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1165: both arguments constants
result = (1073741823 << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1166: LHS constant
y = 27;
result = (1073741823 << y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1167: RHS constant
x = 1073741823;
result = (x << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1168: both arguments variables
x = 1073741824;
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1169: both arguments constants
result = (1073741824 << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1170: LHS constant
y = 27;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1171: RHS constant
x = 1073741824;
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1172: both arguments variables
x = 1073741825;
y = 27;
result = (x << y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1173: both arguments constants
result = (1073741825 << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1174: LHS constant
y = 27;
result = (1073741825 << y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1175: RHS constant
x = 1073741825;
result = (x << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1176: both arguments variables
x = -1073741823;
y = 27;
result = (x << y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1177: both arguments constants
result = (-1073741823 << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1178: LHS constant
y = 27;
result = (-1073741823 << y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1179: RHS constant
x = -1073741823;
result = (x << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1180: both arguments variables
x = (-0x3fffffff-1);
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1181: both arguments constants
result = ((-0x3fffffff-1) << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1182: LHS constant
y = 27;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1183: RHS constant
x = (-0x3fffffff-1);
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1184: both arguments variables
x = -1073741825;
y = 27;
result = (x << y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1185: both arguments constants
result = (-1073741825 << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1186: LHS constant
y = 27;
result = (-1073741825 << y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1187: RHS constant
x = -1073741825;
result = (x << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1188: both arguments variables
x = -1073741826;
y = 27;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1189: both arguments constants
result = (-1073741826 << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1190: LHS constant
y = 27;
result = (-1073741826 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1191: RHS constant
x = -1073741826;
result = (x << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1192: both arguments variables
x = 2147483646;
y = 27;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1193: both arguments constants
result = (2147483646 << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1194: LHS constant
y = 27;
result = (2147483646 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1195: RHS constant
x = 2147483646;
result = (x << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1196: both arguments variables
x = 2147483647;
y = 27;
result = (x << y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1197: both arguments constants
result = (2147483647 << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1198: LHS constant
y = 27;
result = (2147483647 << y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1199: RHS constant
x = 2147483647;
result = (x << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test12() {
var x;
var y;
var result;
var check;
// Test 1200: both arguments variables
x = 2147483648;
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1201: both arguments constants
result = (2147483648 << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1202: LHS constant
y = 27;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1203: RHS constant
x = 2147483648;
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1204: both arguments variables
x = 2147483649;
y = 27;
result = (x << y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1205: both arguments constants
result = (2147483649 << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1206: LHS constant
y = 27;
result = (2147483649 << y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1207: RHS constant
x = 2147483649;
result = (x << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1208: both arguments variables
x = -2147483647;
y = 27;
result = (x << y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1209: both arguments constants
result = (-2147483647 << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1210: LHS constant
y = 27;
result = (-2147483647 << y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1211: RHS constant
x = -2147483647;
result = (x << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1212: both arguments variables
x = -2147483648;
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1213: both arguments constants
result = (-2147483648 << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1214: LHS constant
y = 27;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1215: RHS constant
x = -2147483648;
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1216: both arguments variables
x = -2147483649;
y = 27;
result = (x << y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1217: both arguments constants
result = (-2147483649 << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1218: LHS constant
y = 27;
result = (-2147483649 << y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1219: RHS constant
x = -2147483649;
result = (x << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1220: both arguments variables
x = -2147483650;
y = 27;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1221: both arguments constants
result = (-2147483650 << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1222: LHS constant
y = 27;
result = (-2147483650 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1223: RHS constant
x = -2147483650;
result = (x << 27)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1224: both arguments variables
x = 4294967295;
y = 27;
result = (x << y);
check = -134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1225: both arguments constants
result = (4294967295 << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1226: LHS constant
y = 27;
result = (4294967295 << y)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1227: RHS constant
x = 4294967295;
result = (x << 27)
check = -134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1228: both arguments variables
x = 4294967296;
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1229: both arguments constants
result = (4294967296 << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1230: LHS constant
y = 27;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1231: RHS constant
x = 4294967296;
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1232: both arguments variables
x = -4294967295;
y = 27;
result = (x << y);
check = 134217728;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1233: both arguments constants
result = (-4294967295 << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1234: LHS constant
y = 27;
result = (-4294967295 << y)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1235: RHS constant
x = -4294967295;
result = (x << 27)
check = 134217728
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1236: both arguments variables
x = -4294967296;
y = 27;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1237: both arguments constants
result = (-4294967296 << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1238: LHS constant
y = 27;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1239: RHS constant
x = -4294967296;
result = (x << 27)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1240: both arguments variables
x = 0;
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1241: both arguments constants
result = (0 << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1242: LHS constant
y = 28;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1243: RHS constant
x = 0;
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1244: both arguments variables
x = 1;
y = 28;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1245: both arguments constants
result = (1 << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1246: LHS constant
y = 28;
result = (1 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1247: RHS constant
x = 1;
result = (x << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1248: both arguments variables
x = -1;
y = 28;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1249: both arguments constants
result = (-1 << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1250: LHS constant
y = 28;
result = (-1 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1251: RHS constant
x = -1;
result = (x << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1252: both arguments variables
x = 2;
y = 28;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1253: both arguments constants
result = (2 << 28)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1254: LHS constant
y = 28;
result = (2 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1255: RHS constant
x = 2;
result = (x << 28)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1256: both arguments variables
x = -2;
y = 28;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1257: both arguments constants
result = (-2 << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1258: LHS constant
y = 28;
result = (-2 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1259: RHS constant
x = -2;
result = (x << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1260: both arguments variables
x = 3;
y = 28;
result = (x << y);
check = 805306368;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1261: both arguments constants
result = (3 << 28)
check = 805306368
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1262: LHS constant
y = 28;
result = (3 << y)
check = 805306368
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1263: RHS constant
x = 3;
result = (x << 28)
check = 805306368
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1264: both arguments variables
x = -3;
y = 28;
result = (x << y);
check = -805306368;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1265: both arguments constants
result = (-3 << 28)
check = -805306368
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1266: LHS constant
y = 28;
result = (-3 << y)
check = -805306368
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1267: RHS constant
x = -3;
result = (x << 28)
check = -805306368
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1268: both arguments variables
x = 4;
y = 28;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1269: both arguments constants
result = (4 << 28)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1270: LHS constant
y = 28;
result = (4 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1271: RHS constant
x = 4;
result = (x << 28)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1272: both arguments variables
x = -4;
y = 28;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1273: both arguments constants
result = (-4 << 28)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1274: LHS constant
y = 28;
result = (-4 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1275: RHS constant
x = -4;
result = (x << 28)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1276: both arguments variables
x = 8;
y = 28;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1277: both arguments constants
result = (8 << 28)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1278: LHS constant
y = 28;
result = (8 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1279: RHS constant
x = 8;
result = (x << 28)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1280: both arguments variables
x = -8;
y = 28;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1281: both arguments constants
result = (-8 << 28)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1282: LHS constant
y = 28;
result = (-8 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1283: RHS constant
x = -8;
result = (x << 28)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1284: both arguments variables
x = 1073741822;
y = 28;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1285: both arguments constants
result = (1073741822 << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1286: LHS constant
y = 28;
result = (1073741822 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1287: RHS constant
x = 1073741822;
result = (x << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1288: both arguments variables
x = 1073741823;
y = 28;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1289: both arguments constants
result = (1073741823 << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1290: LHS constant
y = 28;
result = (1073741823 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1291: RHS constant
x = 1073741823;
result = (x << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1292: both arguments variables
x = 1073741824;
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1293: both arguments constants
result = (1073741824 << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1294: LHS constant
y = 28;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1295: RHS constant
x = 1073741824;
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1296: both arguments variables
x = 1073741825;
y = 28;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1297: both arguments constants
result = (1073741825 << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1298: LHS constant
y = 28;
result = (1073741825 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1299: RHS constant
x = 1073741825;
result = (x << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test13() {
var x;
var y;
var result;
var check;
// Test 1300: both arguments variables
x = -1073741823;
y = 28;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1301: both arguments constants
result = (-1073741823 << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1302: LHS constant
y = 28;
result = (-1073741823 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1303: RHS constant
x = -1073741823;
result = (x << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1304: both arguments variables
x = (-0x3fffffff-1);
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1305: both arguments constants
result = ((-0x3fffffff-1) << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1306: LHS constant
y = 28;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1307: RHS constant
x = (-0x3fffffff-1);
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1308: both arguments variables
x = -1073741825;
y = 28;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1309: both arguments constants
result = (-1073741825 << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1310: LHS constant
y = 28;
result = (-1073741825 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1311: RHS constant
x = -1073741825;
result = (x << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1312: both arguments variables
x = -1073741826;
y = 28;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1313: both arguments constants
result = (-1073741826 << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1314: LHS constant
y = 28;
result = (-1073741826 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1315: RHS constant
x = -1073741826;
result = (x << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1316: both arguments variables
x = 2147483646;
y = 28;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1317: both arguments constants
result = (2147483646 << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1318: LHS constant
y = 28;
result = (2147483646 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1319: RHS constant
x = 2147483646;
result = (x << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1320: both arguments variables
x = 2147483647;
y = 28;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1321: both arguments constants
result = (2147483647 << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1322: LHS constant
y = 28;
result = (2147483647 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1323: RHS constant
x = 2147483647;
result = (x << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1324: both arguments variables
x = 2147483648;
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1325: both arguments constants
result = (2147483648 << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1326: LHS constant
y = 28;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1327: RHS constant
x = 2147483648;
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1328: both arguments variables
x = 2147483649;
y = 28;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1329: both arguments constants
result = (2147483649 << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1330: LHS constant
y = 28;
result = (2147483649 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1331: RHS constant
x = 2147483649;
result = (x << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1332: both arguments variables
x = -2147483647;
y = 28;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1333: both arguments constants
result = (-2147483647 << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1334: LHS constant
y = 28;
result = (-2147483647 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1335: RHS constant
x = -2147483647;
result = (x << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1336: both arguments variables
x = -2147483648;
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1337: both arguments constants
result = (-2147483648 << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1338: LHS constant
y = 28;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1339: RHS constant
x = -2147483648;
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1340: both arguments variables
x = -2147483649;
y = 28;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1341: both arguments constants
result = (-2147483649 << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1342: LHS constant
y = 28;
result = (-2147483649 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1343: RHS constant
x = -2147483649;
result = (x << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1344: both arguments variables
x = -2147483650;
y = 28;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1345: both arguments constants
result = (-2147483650 << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1346: LHS constant
y = 28;
result = (-2147483650 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1347: RHS constant
x = -2147483650;
result = (x << 28)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1348: both arguments variables
x = 4294967295;
y = 28;
result = (x << y);
check = -268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1349: both arguments constants
result = (4294967295 << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1350: LHS constant
y = 28;
result = (4294967295 << y)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1351: RHS constant
x = 4294967295;
result = (x << 28)
check = -268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1352: both arguments variables
x = 4294967296;
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1353: both arguments constants
result = (4294967296 << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1354: LHS constant
y = 28;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1355: RHS constant
x = 4294967296;
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1356: both arguments variables
x = -4294967295;
y = 28;
result = (x << y);
check = 268435456;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1357: both arguments constants
result = (-4294967295 << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1358: LHS constant
y = 28;
result = (-4294967295 << y)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1359: RHS constant
x = -4294967295;
result = (x << 28)
check = 268435456
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1360: both arguments variables
x = -4294967296;
y = 28;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1361: both arguments constants
result = (-4294967296 << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1362: LHS constant
y = 28;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1363: RHS constant
x = -4294967296;
result = (x << 28)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1364: both arguments variables
x = 0;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1365: both arguments constants
result = (0 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1366: LHS constant
y = 29;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1367: RHS constant
x = 0;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1368: both arguments variables
x = 1;
y = 29;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1369: both arguments constants
result = (1 << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1370: LHS constant
y = 29;
result = (1 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1371: RHS constant
x = 1;
result = (x << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1372: both arguments variables
x = -1;
y = 29;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1373: both arguments constants
result = (-1 << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1374: LHS constant
y = 29;
result = (-1 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1375: RHS constant
x = -1;
result = (x << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1376: both arguments variables
x = 2;
y = 29;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1377: both arguments constants
result = (2 << 29)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1378: LHS constant
y = 29;
result = (2 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1379: RHS constant
x = 2;
result = (x << 29)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1380: both arguments variables
x = -2;
y = 29;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1381: both arguments constants
result = (-2 << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1382: LHS constant
y = 29;
result = (-2 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1383: RHS constant
x = -2;
result = (x << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1384: both arguments variables
x = 3;
y = 29;
result = (x << y);
check = 1610612736;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1385: both arguments constants
result = (3 << 29)
check = 1610612736
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1386: LHS constant
y = 29;
result = (3 << y)
check = 1610612736
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1387: RHS constant
x = 3;
result = (x << 29)
check = 1610612736
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1388: both arguments variables
x = -3;
y = 29;
result = (x << y);
check = -1610612736;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1389: both arguments constants
result = (-3 << 29)
check = -1610612736
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1390: LHS constant
y = 29;
result = (-3 << y)
check = -1610612736
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1391: RHS constant
x = -3;
result = (x << 29)
check = -1610612736
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1392: both arguments variables
x = 4;
y = 29;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1393: both arguments constants
result = (4 << 29)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1394: LHS constant
y = 29;
result = (4 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1395: RHS constant
x = 4;
result = (x << 29)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1396: both arguments variables
x = -4;
y = 29;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1397: both arguments constants
result = (-4 << 29)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1398: LHS constant
y = 29;
result = (-4 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1399: RHS constant
x = -4;
result = (x << 29)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test14() {
var x;
var y;
var result;
var check;
// Test 1400: both arguments variables
x = 8;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1401: both arguments constants
result = (8 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1402: LHS constant
y = 29;
result = (8 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1403: RHS constant
x = 8;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1404: both arguments variables
x = -8;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1405: both arguments constants
result = (-8 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1406: LHS constant
y = 29;
result = (-8 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1407: RHS constant
x = -8;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1408: both arguments variables
x = 1073741822;
y = 29;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1409: both arguments constants
result = (1073741822 << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1410: LHS constant
y = 29;
result = (1073741822 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1411: RHS constant
x = 1073741822;
result = (x << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1412: both arguments variables
x = 1073741823;
y = 29;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1413: both arguments constants
result = (1073741823 << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1414: LHS constant
y = 29;
result = (1073741823 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1415: RHS constant
x = 1073741823;
result = (x << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1416: both arguments variables
x = 1073741824;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1417: both arguments constants
result = (1073741824 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1418: LHS constant
y = 29;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1419: RHS constant
x = 1073741824;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1420: both arguments variables
x = 1073741825;
y = 29;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1421: both arguments constants
result = (1073741825 << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1422: LHS constant
y = 29;
result = (1073741825 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1423: RHS constant
x = 1073741825;
result = (x << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1424: both arguments variables
x = -1073741823;
y = 29;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1425: both arguments constants
result = (-1073741823 << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1426: LHS constant
y = 29;
result = (-1073741823 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1427: RHS constant
x = -1073741823;
result = (x << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1428: both arguments variables
x = (-0x3fffffff-1);
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1429: both arguments constants
result = ((-0x3fffffff-1) << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1430: LHS constant
y = 29;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1431: RHS constant
x = (-0x3fffffff-1);
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1432: both arguments variables
x = -1073741825;
y = 29;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1433: both arguments constants
result = (-1073741825 << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1434: LHS constant
y = 29;
result = (-1073741825 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1435: RHS constant
x = -1073741825;
result = (x << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1436: both arguments variables
x = -1073741826;
y = 29;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1437: both arguments constants
result = (-1073741826 << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1438: LHS constant
y = 29;
result = (-1073741826 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1439: RHS constant
x = -1073741826;
result = (x << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1440: both arguments variables
x = 2147483646;
y = 29;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1441: both arguments constants
result = (2147483646 << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1442: LHS constant
y = 29;
result = (2147483646 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1443: RHS constant
x = 2147483646;
result = (x << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1444: both arguments variables
x = 2147483647;
y = 29;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1445: both arguments constants
result = (2147483647 << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1446: LHS constant
y = 29;
result = (2147483647 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1447: RHS constant
x = 2147483647;
result = (x << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1448: both arguments variables
x = 2147483648;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1449: both arguments constants
result = (2147483648 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1450: LHS constant
y = 29;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1451: RHS constant
x = 2147483648;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1452: both arguments variables
x = 2147483649;
y = 29;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1453: both arguments constants
result = (2147483649 << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1454: LHS constant
y = 29;
result = (2147483649 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1455: RHS constant
x = 2147483649;
result = (x << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1456: both arguments variables
x = -2147483647;
y = 29;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1457: both arguments constants
result = (-2147483647 << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1458: LHS constant
y = 29;
result = (-2147483647 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1459: RHS constant
x = -2147483647;
result = (x << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1460: both arguments variables
x = -2147483648;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1461: both arguments constants
result = (-2147483648 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1462: LHS constant
y = 29;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1463: RHS constant
x = -2147483648;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1464: both arguments variables
x = -2147483649;
y = 29;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1465: both arguments constants
result = (-2147483649 << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1466: LHS constant
y = 29;
result = (-2147483649 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1467: RHS constant
x = -2147483649;
result = (x << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1468: both arguments variables
x = -2147483650;
y = 29;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1469: both arguments constants
result = (-2147483650 << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1470: LHS constant
y = 29;
result = (-2147483650 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1471: RHS constant
x = -2147483650;
result = (x << 29)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1472: both arguments variables
x = 4294967295;
y = 29;
result = (x << y);
check = -536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1473: both arguments constants
result = (4294967295 << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1474: LHS constant
y = 29;
result = (4294967295 << y)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1475: RHS constant
x = 4294967295;
result = (x << 29)
check = -536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1476: both arguments variables
x = 4294967296;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1477: both arguments constants
result = (4294967296 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1478: LHS constant
y = 29;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1479: RHS constant
x = 4294967296;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1480: both arguments variables
x = -4294967295;
y = 29;
result = (x << y);
check = 536870912;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1481: both arguments constants
result = (-4294967295 << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1482: LHS constant
y = 29;
result = (-4294967295 << y)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1483: RHS constant
x = -4294967295;
result = (x << 29)
check = 536870912
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1484: both arguments variables
x = -4294967296;
y = 29;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1485: both arguments constants
result = (-4294967296 << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1486: LHS constant
y = 29;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1487: RHS constant
x = -4294967296;
result = (x << 29)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1488: both arguments variables
x = 0;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1489: both arguments constants
result = (0 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1490: LHS constant
y = 30;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1491: RHS constant
x = 0;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1492: both arguments variables
x = 1;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1493: both arguments constants
result = (1 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1494: LHS constant
y = 30;
result = (1 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1495: RHS constant
x = 1;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1496: both arguments variables
x = -1;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1497: both arguments constants
result = (-1 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1498: LHS constant
y = 30;
result = (-1 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1499: RHS constant
x = -1;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test15() {
var x;
var y;
var result;
var check;
// Test 1500: both arguments variables
x = 2;
y = 30;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1501: both arguments constants
result = (2 << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1502: LHS constant
y = 30;
result = (2 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1503: RHS constant
x = 2;
result = (x << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1504: both arguments variables
x = -2;
y = 30;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1505: both arguments constants
result = (-2 << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1506: LHS constant
y = 30;
result = (-2 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1507: RHS constant
x = -2;
result = (x << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1508: both arguments variables
x = 3;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1509: both arguments constants
result = (3 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1510: LHS constant
y = 30;
result = (3 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1511: RHS constant
x = 3;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1512: both arguments variables
x = -3;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1513: both arguments constants
result = (-3 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1514: LHS constant
y = 30;
result = (-3 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1515: RHS constant
x = -3;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1516: both arguments variables
x = 4;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1517: both arguments constants
result = (4 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1518: LHS constant
y = 30;
result = (4 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1519: RHS constant
x = 4;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1520: both arguments variables
x = -4;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1521: both arguments constants
result = (-4 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1522: LHS constant
y = 30;
result = (-4 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1523: RHS constant
x = -4;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1524: both arguments variables
x = 8;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1525: both arguments constants
result = (8 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1526: LHS constant
y = 30;
result = (8 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1527: RHS constant
x = 8;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1528: both arguments variables
x = -8;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1529: both arguments constants
result = (-8 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1530: LHS constant
y = 30;
result = (-8 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1531: RHS constant
x = -8;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1532: both arguments variables
x = 1073741822;
y = 30;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1533: both arguments constants
result = (1073741822 << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1534: LHS constant
y = 30;
result = (1073741822 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1535: RHS constant
x = 1073741822;
result = (x << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1536: both arguments variables
x = 1073741823;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1537: both arguments constants
result = (1073741823 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1538: LHS constant
y = 30;
result = (1073741823 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1539: RHS constant
x = 1073741823;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1540: both arguments variables
x = 1073741824;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1541: both arguments constants
result = (1073741824 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1542: LHS constant
y = 30;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1543: RHS constant
x = 1073741824;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1544: both arguments variables
x = 1073741825;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1545: both arguments constants
result = (1073741825 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1546: LHS constant
y = 30;
result = (1073741825 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1547: RHS constant
x = 1073741825;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1548: both arguments variables
x = -1073741823;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1549: both arguments constants
result = (-1073741823 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1550: LHS constant
y = 30;
result = (-1073741823 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1551: RHS constant
x = -1073741823;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1552: both arguments variables
x = (-0x3fffffff-1);
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1553: both arguments constants
result = ((-0x3fffffff-1) << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1554: LHS constant
y = 30;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1555: RHS constant
x = (-0x3fffffff-1);
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1556: both arguments variables
x = -1073741825;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1557: both arguments constants
result = (-1073741825 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1558: LHS constant
y = 30;
result = (-1073741825 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1559: RHS constant
x = -1073741825;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1560: both arguments variables
x = -1073741826;
y = 30;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1561: both arguments constants
result = (-1073741826 << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1562: LHS constant
y = 30;
result = (-1073741826 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1563: RHS constant
x = -1073741826;
result = (x << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1564: both arguments variables
x = 2147483646;
y = 30;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1565: both arguments constants
result = (2147483646 << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1566: LHS constant
y = 30;
result = (2147483646 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1567: RHS constant
x = 2147483646;
result = (x << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1568: both arguments variables
x = 2147483647;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1569: both arguments constants
result = (2147483647 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1570: LHS constant
y = 30;
result = (2147483647 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1571: RHS constant
x = 2147483647;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1572: both arguments variables
x = 2147483648;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1573: both arguments constants
result = (2147483648 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1574: LHS constant
y = 30;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1575: RHS constant
x = 2147483648;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1576: both arguments variables
x = 2147483649;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1577: both arguments constants
result = (2147483649 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1578: LHS constant
y = 30;
result = (2147483649 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1579: RHS constant
x = 2147483649;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1580: both arguments variables
x = -2147483647;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1581: both arguments constants
result = (-2147483647 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1582: LHS constant
y = 30;
result = (-2147483647 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1583: RHS constant
x = -2147483647;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1584: both arguments variables
x = -2147483648;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1585: both arguments constants
result = (-2147483648 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1586: LHS constant
y = 30;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1587: RHS constant
x = -2147483648;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1588: both arguments variables
x = -2147483649;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1589: both arguments constants
result = (-2147483649 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1590: LHS constant
y = 30;
result = (-2147483649 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1591: RHS constant
x = -2147483649;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1592: both arguments variables
x = -2147483650;
y = 30;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1593: both arguments constants
result = (-2147483650 << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1594: LHS constant
y = 30;
result = (-2147483650 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1595: RHS constant
x = -2147483650;
result = (x << 30)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1596: both arguments variables
x = 4294967295;
y = 30;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1597: both arguments constants
result = (4294967295 << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1598: LHS constant
y = 30;
result = (4294967295 << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1599: RHS constant
x = 4294967295;
result = (x << 30)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test16() {
var x;
var y;
var result;
var check;
// Test 1600: both arguments variables
x = 4294967296;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1601: both arguments constants
result = (4294967296 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1602: LHS constant
y = 30;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1603: RHS constant
x = 4294967296;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1604: both arguments variables
x = -4294967295;
y = 30;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1605: both arguments constants
result = (-4294967295 << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1606: LHS constant
y = 30;
result = (-4294967295 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1607: RHS constant
x = -4294967295;
result = (x << 30)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1608: both arguments variables
x = -4294967296;
y = 30;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1609: both arguments constants
result = (-4294967296 << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1610: LHS constant
y = 30;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1611: RHS constant
x = -4294967296;
result = (x << 30)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1612: both arguments variables
x = 0;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1613: both arguments constants
result = (0 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1614: LHS constant
y = 31;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1615: RHS constant
x = 0;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1616: both arguments variables
x = 1;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1617: both arguments constants
result = (1 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1618: LHS constant
y = 31;
result = (1 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1619: RHS constant
x = 1;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1620: both arguments variables
x = -1;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1621: both arguments constants
result = (-1 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1622: LHS constant
y = 31;
result = (-1 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1623: RHS constant
x = -1;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1624: both arguments variables
x = 2;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1625: both arguments constants
result = (2 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1626: LHS constant
y = 31;
result = (2 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1627: RHS constant
x = 2;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1628: both arguments variables
x = -2;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1629: both arguments constants
result = (-2 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1630: LHS constant
y = 31;
result = (-2 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1631: RHS constant
x = -2;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1632: both arguments variables
x = 3;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1633: both arguments constants
result = (3 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1634: LHS constant
y = 31;
result = (3 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1635: RHS constant
x = 3;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1636: both arguments variables
x = -3;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1637: both arguments constants
result = (-3 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1638: LHS constant
y = 31;
result = (-3 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1639: RHS constant
x = -3;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1640: both arguments variables
x = 4;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1641: both arguments constants
result = (4 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1642: LHS constant
y = 31;
result = (4 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1643: RHS constant
x = 4;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1644: both arguments variables
x = -4;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1645: both arguments constants
result = (-4 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1646: LHS constant
y = 31;
result = (-4 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1647: RHS constant
x = -4;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1648: both arguments variables
x = 8;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1649: both arguments constants
result = (8 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1650: LHS constant
y = 31;
result = (8 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1651: RHS constant
x = 8;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1652: both arguments variables
x = -8;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1653: both arguments constants
result = (-8 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1654: LHS constant
y = 31;
result = (-8 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1655: RHS constant
x = -8;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1656: both arguments variables
x = 1073741822;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1657: both arguments constants
result = (1073741822 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1658: LHS constant
y = 31;
result = (1073741822 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1659: RHS constant
x = 1073741822;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1660: both arguments variables
x = 1073741823;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1661: both arguments constants
result = (1073741823 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1662: LHS constant
y = 31;
result = (1073741823 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1663: RHS constant
x = 1073741823;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1664: both arguments variables
x = 1073741824;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1665: both arguments constants
result = (1073741824 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1666: LHS constant
y = 31;
result = (1073741824 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1667: RHS constant
x = 1073741824;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1668: both arguments variables
x = 1073741825;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1669: both arguments constants
result = (1073741825 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1670: LHS constant
y = 31;
result = (1073741825 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1671: RHS constant
x = 1073741825;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1672: both arguments variables
x = -1073741823;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1673: both arguments constants
result = (-1073741823 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1674: LHS constant
y = 31;
result = (-1073741823 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1675: RHS constant
x = -1073741823;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1676: both arguments variables
x = (-0x3fffffff-1);
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1677: both arguments constants
result = ((-0x3fffffff-1) << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1678: LHS constant
y = 31;
result = ((-0x3fffffff-1) << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1679: RHS constant
x = (-0x3fffffff-1);
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1680: both arguments variables
x = -1073741825;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1681: both arguments constants
result = (-1073741825 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1682: LHS constant
y = 31;
result = (-1073741825 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1683: RHS constant
x = -1073741825;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1684: both arguments variables
x = -1073741826;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1685: both arguments constants
result = (-1073741826 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1686: LHS constant
y = 31;
result = (-1073741826 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1687: RHS constant
x = -1073741826;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1688: both arguments variables
x = 2147483646;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1689: both arguments constants
result = (2147483646 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1690: LHS constant
y = 31;
result = (2147483646 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1691: RHS constant
x = 2147483646;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1692: both arguments variables
x = 2147483647;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1693: both arguments constants
result = (2147483647 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1694: LHS constant
y = 31;
result = (2147483647 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1695: RHS constant
x = 2147483647;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1696: both arguments variables
x = 2147483648;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1697: both arguments constants
result = (2147483648 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1698: LHS constant
y = 31;
result = (2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1699: RHS constant
x = 2147483648;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test17() {
var x;
var y;
var result;
var check;
// Test 1700: both arguments variables
x = 2147483649;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1701: both arguments constants
result = (2147483649 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1702: LHS constant
y = 31;
result = (2147483649 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1703: RHS constant
x = 2147483649;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1704: both arguments variables
x = -2147483647;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1705: both arguments constants
result = (-2147483647 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1706: LHS constant
y = 31;
result = (-2147483647 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1707: RHS constant
x = -2147483647;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1708: both arguments variables
x = -2147483648;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1709: both arguments constants
result = (-2147483648 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1710: LHS constant
y = 31;
result = (-2147483648 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1711: RHS constant
x = -2147483648;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1712: both arguments variables
x = -2147483649;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1713: both arguments constants
result = (-2147483649 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1714: LHS constant
y = 31;
result = (-2147483649 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1715: RHS constant
x = -2147483649;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1716: both arguments variables
x = -2147483650;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1717: both arguments constants
result = (-2147483650 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1718: LHS constant
y = 31;
result = (-2147483650 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1719: RHS constant
x = -2147483650;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1720: both arguments variables
x = 4294967295;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1721: both arguments constants
result = (4294967295 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1722: LHS constant
y = 31;
result = (4294967295 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1723: RHS constant
x = 4294967295;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1724: both arguments variables
x = 4294967296;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1725: both arguments constants
result = (4294967296 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1726: LHS constant
y = 31;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1727: RHS constant
x = 4294967296;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1728: both arguments variables
x = -4294967295;
y = 31;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1729: both arguments constants
result = (-4294967295 << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1730: LHS constant
y = 31;
result = (-4294967295 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1731: RHS constant
x = -4294967295;
result = (x << 31)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1732: both arguments variables
x = -4294967296;
y = 31;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1733: both arguments constants
result = (-4294967296 << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1734: LHS constant
y = 31;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1735: RHS constant
x = -4294967296;
result = (x << 31)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1736: both arguments variables
x = 0;
y = 32;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1737: both arguments constants
result = (0 << 32)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1738: LHS constant
y = 32;
result = (0 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1739: RHS constant
x = 0;
result = (x << 32)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1740: both arguments variables
x = 1;
y = 32;
result = (x << y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1741: both arguments constants
result = (1 << 32)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1742: LHS constant
y = 32;
result = (1 << y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1743: RHS constant
x = 1;
result = (x << 32)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1744: both arguments variables
x = -1;
y = 32;
result = (x << y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1745: both arguments constants
result = (-1 << 32)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1746: LHS constant
y = 32;
result = (-1 << y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1747: RHS constant
x = -1;
result = (x << 32)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1748: both arguments variables
x = 2;
y = 32;
result = (x << y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1749: both arguments constants
result = (2 << 32)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1750: LHS constant
y = 32;
result = (2 << y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1751: RHS constant
x = 2;
result = (x << 32)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1752: both arguments variables
x = -2;
y = 32;
result = (x << y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1753: both arguments constants
result = (-2 << 32)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1754: LHS constant
y = 32;
result = (-2 << y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1755: RHS constant
x = -2;
result = (x << 32)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1756: both arguments variables
x = 3;
y = 32;
result = (x << y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1757: both arguments constants
result = (3 << 32)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1758: LHS constant
y = 32;
result = (3 << y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1759: RHS constant
x = 3;
result = (x << 32)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1760: both arguments variables
x = -3;
y = 32;
result = (x << y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1761: both arguments constants
result = (-3 << 32)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1762: LHS constant
y = 32;
result = (-3 << y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1763: RHS constant
x = -3;
result = (x << 32)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1764: both arguments variables
x = 4;
y = 32;
result = (x << y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1765: both arguments constants
result = (4 << 32)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1766: LHS constant
y = 32;
result = (4 << y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1767: RHS constant
x = 4;
result = (x << 32)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1768: both arguments variables
x = -4;
y = 32;
result = (x << y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1769: both arguments constants
result = (-4 << 32)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1770: LHS constant
y = 32;
result = (-4 << y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1771: RHS constant
x = -4;
result = (x << 32)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1772: both arguments variables
x = 8;
y = 32;
result = (x << y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1773: both arguments constants
result = (8 << 32)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1774: LHS constant
y = 32;
result = (8 << y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1775: RHS constant
x = 8;
result = (x << 32)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1776: both arguments variables
x = -8;
y = 32;
result = (x << y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1777: both arguments constants
result = (-8 << 32)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1778: LHS constant
y = 32;
result = (-8 << y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1779: RHS constant
x = -8;
result = (x << 32)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1780: both arguments variables
x = 1073741822;
y = 32;
result = (x << y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1781: both arguments constants
result = (1073741822 << 32)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1782: LHS constant
y = 32;
result = (1073741822 << y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1783: RHS constant
x = 1073741822;
result = (x << 32)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1784: both arguments variables
x = 1073741823;
y = 32;
result = (x << y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1785: both arguments constants
result = (1073741823 << 32)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1786: LHS constant
y = 32;
result = (1073741823 << y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1787: RHS constant
x = 1073741823;
result = (x << 32)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1788: both arguments variables
x = 1073741824;
y = 32;
result = (x << y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1789: both arguments constants
result = (1073741824 << 32)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1790: LHS constant
y = 32;
result = (1073741824 << y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1791: RHS constant
x = 1073741824;
result = (x << 32)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1792: both arguments variables
x = 1073741825;
y = 32;
result = (x << y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1793: both arguments constants
result = (1073741825 << 32)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1794: LHS constant
y = 32;
result = (1073741825 << y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1795: RHS constant
x = 1073741825;
result = (x << 32)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1796: both arguments variables
x = -1073741823;
y = 32;
result = (x << y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1797: both arguments constants
result = (-1073741823 << 32)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1798: LHS constant
y = 32;
result = (-1073741823 << y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1799: RHS constant
x = -1073741823;
result = (x << 32)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test18() {
var x;
var y;
var result;
var check;
// Test 1800: both arguments variables
x = (-0x3fffffff-1);
y = 32;
result = (x << y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1801: both arguments constants
result = ((-0x3fffffff-1) << 32)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1802: LHS constant
y = 32;
result = ((-0x3fffffff-1) << y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1803: RHS constant
x = (-0x3fffffff-1);
result = (x << 32)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1804: both arguments variables
x = -1073741825;
y = 32;
result = (x << y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1805: both arguments constants
result = (-1073741825 << 32)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1806: LHS constant
y = 32;
result = (-1073741825 << y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1807: RHS constant
x = -1073741825;
result = (x << 32)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1808: both arguments variables
x = -1073741826;
y = 32;
result = (x << y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1809: both arguments constants
result = (-1073741826 << 32)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1810: LHS constant
y = 32;
result = (-1073741826 << y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1811: RHS constant
x = -1073741826;
result = (x << 32)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1812: both arguments variables
x = 2147483646;
y = 32;
result = (x << y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1813: both arguments constants
result = (2147483646 << 32)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1814: LHS constant
y = 32;
result = (2147483646 << y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1815: RHS constant
x = 2147483646;
result = (x << 32)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1816: both arguments variables
x = 2147483647;
y = 32;
result = (x << y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1817: both arguments constants
result = (2147483647 << 32)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1818: LHS constant
y = 32;
result = (2147483647 << y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1819: RHS constant
x = 2147483647;
result = (x << 32)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1820: both arguments variables
x = 2147483648;
y = 32;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1821: both arguments constants
result = (2147483648 << 32)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1822: LHS constant
y = 32;
result = (2147483648 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1823: RHS constant
x = 2147483648;
result = (x << 32)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1824: both arguments variables
x = 2147483649;
y = 32;
result = (x << y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1825: both arguments constants
result = (2147483649 << 32)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1826: LHS constant
y = 32;
result = (2147483649 << y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1827: RHS constant
x = 2147483649;
result = (x << 32)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1828: both arguments variables
x = -2147483647;
y = 32;
result = (x << y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1829: both arguments constants
result = (-2147483647 << 32)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1830: LHS constant
y = 32;
result = (-2147483647 << y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1831: RHS constant
x = -2147483647;
result = (x << 32)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1832: both arguments variables
x = -2147483648;
y = 32;
result = (x << y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1833: both arguments constants
result = (-2147483648 << 32)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1834: LHS constant
y = 32;
result = (-2147483648 << y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1835: RHS constant
x = -2147483648;
result = (x << 32)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1836: both arguments variables
x = -2147483649;
y = 32;
result = (x << y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1837: both arguments constants
result = (-2147483649 << 32)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1838: LHS constant
y = 32;
result = (-2147483649 << y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1839: RHS constant
x = -2147483649;
result = (x << 32)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1840: both arguments variables
x = -2147483650;
y = 32;
result = (x << y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1841: both arguments constants
result = (-2147483650 << 32)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1842: LHS constant
y = 32;
result = (-2147483650 << y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1843: RHS constant
x = -2147483650;
result = (x << 32)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1844: both arguments variables
x = 4294967295;
y = 32;
result = (x << y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1845: both arguments constants
result = (4294967295 << 32)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1846: LHS constant
y = 32;
result = (4294967295 << y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1847: RHS constant
x = 4294967295;
result = (x << 32)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1848: both arguments variables
x = 4294967296;
y = 32;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1849: both arguments constants
result = (4294967296 << 32)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1850: LHS constant
y = 32;
result = (4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1851: RHS constant
x = 4294967296;
result = (x << 32)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1852: both arguments variables
x = -4294967295;
y = 32;
result = (x << y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1853: both arguments constants
result = (-4294967295 << 32)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1854: LHS constant
y = 32;
result = (-4294967295 << y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1855: RHS constant
x = -4294967295;
result = (x << 32)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1856: both arguments variables
x = -4294967296;
y = 32;
result = (x << y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1857: both arguments constants
result = (-4294967296 << 32)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1858: LHS constant
y = 32;
result = (-4294967296 << y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 1859: RHS constant
x = -4294967296;
result = (x << 32)
check = 0
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
WScript.Echo("done");
