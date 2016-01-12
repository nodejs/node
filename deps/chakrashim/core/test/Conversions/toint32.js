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
x = 0.4;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 1: both arguments constants
result = (0.4 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 2: LHS constant
y = 0;
result = (0.4 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 3: RHS constant
x = 0.4;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 4: both arguments variables
x = 0.5;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 5: both arguments constants
result = (0.5 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 6: LHS constant
y = 0;
result = (0.5 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 7: RHS constant
x = 0.5;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 8: both arguments variables
x = 0.6;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 9: both arguments constants
result = (0.6 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 10: LHS constant
y = 0;
result = (0.6 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 11: RHS constant
x = 0.6;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 12: both arguments variables
x = -0.4;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 13: both arguments constants
result = (-0.4 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 14: LHS constant
y = 0;
result = (-0.4 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 15: RHS constant
x = -0.4;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 16: both arguments variables
x = -0.5;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 17: both arguments constants
result = (-0.5 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 18: LHS constant
y = 0;
result = (-0.5 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 19: RHS constant
x = -0.5;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 20: both arguments variables
x = -0.6;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 21: both arguments constants
result = (-0.6 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 22: LHS constant
y = 0;
result = (-0.6 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 23: RHS constant
x = -0.6;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 24: both arguments variables
x = 1.4;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 25: both arguments constants
result = (1.4 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 26: LHS constant
y = 0;
result = (1.4 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 27: RHS constant
x = 1.4;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 28: both arguments variables
x = 1.5;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 29: both arguments constants
result = (1.5 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 30: LHS constant
y = 0;
result = (1.5 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 31: RHS constant
x = 1.5;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 32: both arguments variables
x = 1.6;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 33: both arguments constants
result = (1.6 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 34: LHS constant
y = 0;
result = (1.6 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 35: RHS constant
x = 1.6;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 36: both arguments variables
x = 0.6;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 37: both arguments constants
result = (0.6 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 38: LHS constant
y = 0;
result = (0.6 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 39: RHS constant
x = 0.6;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 40: both arguments variables
x = 0.5;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 41: both arguments constants
result = (0.5 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 42: LHS constant
y = 0;
result = (0.5 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 43: RHS constant
x = 0.5;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 44: both arguments variables
x = 0.4;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 45: both arguments constants
result = (0.4 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 46: LHS constant
y = 0;
result = (0.4 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 47: RHS constant
x = 0.4;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 48: both arguments variables
x = -0.6;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 49: both arguments constants
result = (-0.6 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 50: LHS constant
y = 0;
result = (-0.6 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 51: RHS constant
x = -0.6;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 52: both arguments variables
x = -0.5;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 53: both arguments constants
result = (-0.5 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 54: LHS constant
y = 0;
result = (-0.5 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 55: RHS constant
x = -0.5;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 56: both arguments variables
x = -0.4;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 57: both arguments constants
result = (-0.4 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 58: LHS constant
y = 0;
result = (-0.4 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 59: RHS constant
x = -0.4;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 60: both arguments variables
x = -1.4;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 61: both arguments constants
result = (-1.4 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 62: LHS constant
y = 0;
result = (-1.4 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 63: RHS constant
x = -1.4;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 64: both arguments variables
x = -1.5;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 65: both arguments constants
result = (-1.5 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 66: LHS constant
y = 0;
result = (-1.5 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 67: RHS constant
x = -1.5;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 68: both arguments variables
x = -1.6;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 69: both arguments constants
result = (-1.6 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 70: LHS constant
y = 0;
result = (-1.6 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 71: RHS constant
x = -1.6;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 72: both arguments variables
x = 2.4;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 73: both arguments constants
result = (2.4 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 74: LHS constant
y = 0;
result = (2.4 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 75: RHS constant
x = 2.4;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 76: both arguments variables
x = 2.5;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 77: both arguments constants
result = (2.5 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 78: LHS constant
y = 0;
result = (2.5 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 79: RHS constant
x = 2.5;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 80: both arguments variables
x = 2.6;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 81: both arguments constants
result = (2.6 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 82: LHS constant
y = 0;
result = (2.6 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 83: RHS constant
x = 2.6;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 84: both arguments variables
x = 1.6;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 85: both arguments constants
result = (1.6 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 86: LHS constant
y = 0;
result = (1.6 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 87: RHS constant
x = 1.6;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 88: both arguments variables
x = 1.5;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 89: both arguments constants
result = (1.5 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 90: LHS constant
y = 0;
result = (1.5 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 91: RHS constant
x = 1.5;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 92: both arguments variables
x = 1.4;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 93: both arguments constants
result = (1.4 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 94: LHS constant
y = 0;
result = (1.4 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 95: RHS constant
x = 1.4;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 96: both arguments variables
x = -1.6;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 97: both arguments constants
result = (-1.6 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 98: LHS constant
y = 0;
result = (-1.6 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 99: RHS constant
x = -1.6;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test1() {
var x;
var y;
var result;
var check;
// Test 100: both arguments variables
x = -1.5;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 101: both arguments constants
result = (-1.5 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 102: LHS constant
y = 0;
result = (-1.5 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 103: RHS constant
x = -1.5;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 104: both arguments variables
x = -1.4;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 105: both arguments constants
result = (-1.4 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 106: LHS constant
y = 0;
result = (-1.4 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 107: RHS constant
x = -1.4;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 108: both arguments variables
x = -2.4;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 109: both arguments constants
result = (-2.4 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 110: LHS constant
y = 0;
result = (-2.4 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 111: RHS constant
x = -2.4;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 112: both arguments variables
x = -2.5;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 113: both arguments constants
result = (-2.5 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 114: LHS constant
y = 0;
result = (-2.5 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 115: RHS constant
x = -2.5;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 116: both arguments variables
x = -2.6;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 117: both arguments constants
result = (-2.6 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 118: LHS constant
y = 0;
result = (-2.6 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 119: RHS constant
x = -2.6;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 120: both arguments variables
x = 3.4;
y = 0;
result = (x >> y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 121: both arguments constants
result = (3.4 >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 122: LHS constant
y = 0;
result = (3.4 >> y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 123: RHS constant
x = 3.4;
result = (x >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 124: both arguments variables
x = 3.5;
y = 0;
result = (x >> y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 125: both arguments constants
result = (3.5 >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 126: LHS constant
y = 0;
result = (3.5 >> y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 127: RHS constant
x = 3.5;
result = (x >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 128: both arguments variables
x = 3.6;
y = 0;
result = (x >> y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 129: both arguments constants
result = (3.6 >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 130: LHS constant
y = 0;
result = (3.6 >> y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 131: RHS constant
x = 3.6;
result = (x >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 132: both arguments variables
x = 2.6;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 133: both arguments constants
result = (2.6 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 134: LHS constant
y = 0;
result = (2.6 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 135: RHS constant
x = 2.6;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 136: both arguments variables
x = 2.5;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 137: both arguments constants
result = (2.5 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 138: LHS constant
y = 0;
result = (2.5 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 139: RHS constant
x = 2.5;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 140: both arguments variables
x = 2.4;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 141: both arguments constants
result = (2.4 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 142: LHS constant
y = 0;
result = (2.4 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 143: RHS constant
x = 2.4;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 144: both arguments variables
x = -2.6;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 145: both arguments constants
result = (-2.6 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 146: LHS constant
y = 0;
result = (-2.6 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 147: RHS constant
x = -2.6;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 148: both arguments variables
x = -2.5;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 149: both arguments constants
result = (-2.5 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 150: LHS constant
y = 0;
result = (-2.5 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 151: RHS constant
x = -2.5;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 152: both arguments variables
x = -2.4;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 153: both arguments constants
result = (-2.4 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 154: LHS constant
y = 0;
result = (-2.4 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 155: RHS constant
x = -2.4;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 156: both arguments variables
x = -3.4;
y = 0;
result = (x >> y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 157: both arguments constants
result = (-3.4 >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 158: LHS constant
y = 0;
result = (-3.4 >> y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 159: RHS constant
x = -3.4;
result = (x >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 160: both arguments variables
x = -3.5;
y = 0;
result = (x >> y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 161: both arguments constants
result = (-3.5 >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 162: LHS constant
y = 0;
result = (-3.5 >> y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 163: RHS constant
x = -3.5;
result = (x >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 164: both arguments variables
x = -3.6;
y = 0;
result = (x >> y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 165: both arguments constants
result = (-3.6 >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 166: LHS constant
y = 0;
result = (-3.6 >> y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 167: RHS constant
x = -3.6;
result = (x >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 168: both arguments variables
x = 4.4;
y = 0;
result = (x >> y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 169: both arguments constants
result = (4.4 >> 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 170: LHS constant
y = 0;
result = (4.4 >> y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 171: RHS constant
x = 4.4;
result = (x >> 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 172: both arguments variables
x = 4.5;
y = 0;
result = (x >> y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 173: both arguments constants
result = (4.5 >> 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 174: LHS constant
y = 0;
result = (4.5 >> y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 175: RHS constant
x = 4.5;
result = (x >> 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 176: both arguments variables
x = 4.6;
y = 0;
result = (x >> y);
check = 4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 177: both arguments constants
result = (4.6 >> 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 178: LHS constant
y = 0;
result = (4.6 >> y)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 179: RHS constant
x = 4.6;
result = (x >> 0)
check = 4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 180: both arguments variables
x = 3.6;
y = 0;
result = (x >> y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 181: both arguments constants
result = (3.6 >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 182: LHS constant
y = 0;
result = (3.6 >> y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 183: RHS constant
x = 3.6;
result = (x >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 184: both arguments variables
x = 3.5;
y = 0;
result = (x >> y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 185: both arguments constants
result = (3.5 >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 186: LHS constant
y = 0;
result = (3.5 >> y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 187: RHS constant
x = 3.5;
result = (x >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 188: both arguments variables
x = 3.4;
y = 0;
result = (x >> y);
check = 3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 189: both arguments constants
result = (3.4 >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 190: LHS constant
y = 0;
result = (3.4 >> y)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 191: RHS constant
x = 3.4;
result = (x >> 0)
check = 3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 192: both arguments variables
x = -3.6;
y = 0;
result = (x >> y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 193: both arguments constants
result = (-3.6 >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 194: LHS constant
y = 0;
result = (-3.6 >> y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 195: RHS constant
x = -3.6;
result = (x >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 196: both arguments variables
x = -3.5;
y = 0;
result = (x >> y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 197: both arguments constants
result = (-3.5 >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 198: LHS constant
y = 0;
result = (-3.5 >> y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 199: RHS constant
x = -3.5;
result = (x >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test2() {
var x;
var y;
var result;
var check;
// Test 200: both arguments variables
x = -3.4;
y = 0;
result = (x >> y);
check = -3;
if(result != check) { fail(test, check, result); } ++test; 

// Test 201: both arguments constants
result = (-3.4 >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 202: LHS constant
y = 0;
result = (-3.4 >> y)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 203: RHS constant
x = -3.4;
result = (x >> 0)
check = -3
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 204: both arguments variables
x = -4.4;
y = 0;
result = (x >> y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 205: both arguments constants
result = (-4.4 >> 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 206: LHS constant
y = 0;
result = (-4.4 >> y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 207: RHS constant
x = -4.4;
result = (x >> 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 208: both arguments variables
x = -4.5;
y = 0;
result = (x >> y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 209: both arguments constants
result = (-4.5 >> 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 210: LHS constant
y = 0;
result = (-4.5 >> y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 211: RHS constant
x = -4.5;
result = (x >> 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 212: both arguments variables
x = -4.6;
y = 0;
result = (x >> y);
check = -4;
if(result != check) { fail(test, check, result); } ++test; 

// Test 213: both arguments constants
result = (-4.6 >> 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 214: LHS constant
y = 0;
result = (-4.6 >> y)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 215: RHS constant
x = -4.6;
result = (x >> 0)
check = -4
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 216: both arguments variables
x = 8.4;
y = 0;
result = (x >> y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 217: both arguments constants
result = (8.4 >> 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 218: LHS constant
y = 0;
result = (8.4 >> y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 219: RHS constant
x = 8.4;
result = (x >> 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 220: both arguments variables
x = 8.5;
y = 0;
result = (x >> y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 221: both arguments constants
result = (8.5 >> 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 222: LHS constant
y = 0;
result = (8.5 >> y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 223: RHS constant
x = 8.5;
result = (x >> 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 224: both arguments variables
x = 8.6;
y = 0;
result = (x >> y);
check = 8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 225: both arguments constants
result = (8.6 >> 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 226: LHS constant
y = 0;
result = (8.6 >> y)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 227: RHS constant
x = 8.6;
result = (x >> 0)
check = 8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 228: both arguments variables
x = 7.6;
y = 0;
result = (x >> y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 229: both arguments constants
result = (7.6 >> 0)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 230: LHS constant
y = 0;
result = (7.6 >> y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 231: RHS constant
x = 7.6;
result = (x >> 0)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 232: both arguments variables
x = 7.5;
y = 0;
result = (x >> y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 233: both arguments constants
result = (7.5 >> 0)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 234: LHS constant
y = 0;
result = (7.5 >> y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 235: RHS constant
x = 7.5;
result = (x >> 0)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 236: both arguments variables
x = 7.4;
y = 0;
result = (x >> y);
check = 7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 237: both arguments constants
result = (7.4 >> 0)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 238: LHS constant
y = 0;
result = (7.4 >> y)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 239: RHS constant
x = 7.4;
result = (x >> 0)
check = 7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 240: both arguments variables
x = -7.6;
y = 0;
result = (x >> y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 241: both arguments constants
result = (-7.6 >> 0)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 242: LHS constant
y = 0;
result = (-7.6 >> y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 243: RHS constant
x = -7.6;
result = (x >> 0)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 244: both arguments variables
x = -7.5;
y = 0;
result = (x >> y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 245: both arguments constants
result = (-7.5 >> 0)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 246: LHS constant
y = 0;
result = (-7.5 >> y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 247: RHS constant
x = -7.5;
result = (x >> 0)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 248: both arguments variables
x = -7.4;
y = 0;
result = (x >> y);
check = -7;
if(result != check) { fail(test, check, result); } ++test; 

// Test 249: both arguments constants
result = (-7.4 >> 0)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 250: LHS constant
y = 0;
result = (-7.4 >> y)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 251: RHS constant
x = -7.4;
result = (x >> 0)
check = -7
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 252: both arguments variables
x = -8.4;
y = 0;
result = (x >> y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 253: both arguments constants
result = (-8.4 >> 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 254: LHS constant
y = 0;
result = (-8.4 >> y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 255: RHS constant
x = -8.4;
result = (x >> 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 256: both arguments variables
x = -8.5;
y = 0;
result = (x >> y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 257: both arguments constants
result = (-8.5 >> 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 258: LHS constant
y = 0;
result = (-8.5 >> y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 259: RHS constant
x = -8.5;
result = (x >> 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 260: both arguments variables
x = -8.6;
y = 0;
result = (x >> y);
check = -8;
if(result != check) { fail(test, check, result); } ++test; 

// Test 261: both arguments constants
result = (-8.6 >> 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 262: LHS constant
y = 0;
result = (-8.6 >> y)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 263: RHS constant
x = -8.6;
result = (x >> 0)
check = -8
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 264: both arguments variables
x = 1073741822.4;
y = 0;
result = (x >> y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 265: both arguments constants
result = (1073741822.4 >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 266: LHS constant
y = 0;
result = (1073741822.4 >> y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 267: RHS constant
x = 1073741822.4;
result = (x >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 268: both arguments variables
x = 1073741822.5;
y = 0;
result = (x >> y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 269: both arguments constants
result = (1073741822.5 >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 270: LHS constant
y = 0;
result = (1073741822.5 >> y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 271: RHS constant
x = 1073741822.5;
result = (x >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 272: both arguments variables
x = 1073741822.6;
y = 0;
result = (x >> y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 273: both arguments constants
result = (1073741822.6 >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 274: LHS constant
y = 0;
result = (1073741822.6 >> y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 275: RHS constant
x = 1073741822.6;
result = (x >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 276: both arguments variables
x = 1073741821.6;
y = 0;
result = (x >> y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 277: both arguments constants
result = (1073741821.6 >> 0)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 278: LHS constant
y = 0;
result = (1073741821.6 >> y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 279: RHS constant
x = 1073741821.6;
result = (x >> 0)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 280: both arguments variables
x = 1073741821.5;
y = 0;
result = (x >> y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 281: both arguments constants
result = (1073741821.5 >> 0)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 282: LHS constant
y = 0;
result = (1073741821.5 >> y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 283: RHS constant
x = 1073741821.5;
result = (x >> 0)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 284: both arguments variables
x = 1073741821.4;
y = 0;
result = (x >> y);
check = 1073741821;
if(result != check) { fail(test, check, result); } ++test; 

// Test 285: both arguments constants
result = (1073741821.4 >> 0)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 286: LHS constant
y = 0;
result = (1073741821.4 >> y)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 287: RHS constant
x = 1073741821.4;
result = (x >> 0)
check = 1073741821
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 288: both arguments variables
x = 1073741823.4;
y = 0;
result = (x >> y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 289: both arguments constants
result = (1073741823.4 >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 290: LHS constant
y = 0;
result = (1073741823.4 >> y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 291: RHS constant
x = 1073741823.4;
result = (x >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 292: both arguments variables
x = 1073741823.5;
y = 0;
result = (x >> y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 293: both arguments constants
result = (1073741823.5 >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 294: LHS constant
y = 0;
result = (1073741823.5 >> y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 295: RHS constant
x = 1073741823.5;
result = (x >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 296: both arguments variables
x = 1073741823.6;
y = 0;
result = (x >> y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 297: both arguments constants
result = (1073741823.6 >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 298: LHS constant
y = 0;
result = (1073741823.6 >> y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 299: RHS constant
x = 1073741823.6;
result = (x >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test3() {
var x;
var y;
var result;
var check;
// Test 300: both arguments variables
x = 1073741822.6;
y = 0;
result = (x >> y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 301: both arguments constants
result = (1073741822.6 >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 302: LHS constant
y = 0;
result = (1073741822.6 >> y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 303: RHS constant
x = 1073741822.6;
result = (x >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 304: both arguments variables
x = 1073741822.5;
y = 0;
result = (x >> y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 305: both arguments constants
result = (1073741822.5 >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 306: LHS constant
y = 0;
result = (1073741822.5 >> y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 307: RHS constant
x = 1073741822.5;
result = (x >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 308: both arguments variables
x = 1073741822.4;
y = 0;
result = (x >> y);
check = 1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 309: both arguments constants
result = (1073741822.4 >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 310: LHS constant
y = 0;
result = (1073741822.4 >> y)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 311: RHS constant
x = 1073741822.4;
result = (x >> 0)
check = 1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 312: both arguments variables
x = 1073741824.4;
y = 0;
result = (x >> y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 313: both arguments constants
result = (1073741824.4 >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 314: LHS constant
y = 0;
result = (1073741824.4 >> y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 315: RHS constant
x = 1073741824.4;
result = (x >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 316: both arguments variables
x = 1073741824.5;
y = 0;
result = (x >> y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 317: both arguments constants
result = (1073741824.5 >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 318: LHS constant
y = 0;
result = (1073741824.5 >> y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 319: RHS constant
x = 1073741824.5;
result = (x >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 320: both arguments variables
x = 1073741824.6;
y = 0;
result = (x >> y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 321: both arguments constants
result = (1073741824.6 >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 322: LHS constant
y = 0;
result = (1073741824.6 >> y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 323: RHS constant
x = 1073741824.6;
result = (x >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 324: both arguments variables
x = 1073741823.6;
y = 0;
result = (x >> y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 325: both arguments constants
result = (1073741823.6 >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 326: LHS constant
y = 0;
result = (1073741823.6 >> y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 327: RHS constant
x = 1073741823.6;
result = (x >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 328: both arguments variables
x = 1073741823.5;
y = 0;
result = (x >> y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 329: both arguments constants
result = (1073741823.5 >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 330: LHS constant
y = 0;
result = (1073741823.5 >> y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 331: RHS constant
x = 1073741823.5;
result = (x >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 332: both arguments variables
x = 1073741823.4;
y = 0;
result = (x >> y);
check = 1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 333: both arguments constants
result = (1073741823.4 >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 334: LHS constant
y = 0;
result = (1073741823.4 >> y)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 335: RHS constant
x = 1073741823.4;
result = (x >> 0)
check = 1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 336: both arguments variables
x = 1073741825.4;
y = 0;
result = (x >> y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 337: both arguments constants
result = (1073741825.4 >> 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 338: LHS constant
y = 0;
result = (1073741825.4 >> y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 339: RHS constant
x = 1073741825.4;
result = (x >> 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 340: both arguments variables
x = 1073741825.5;
y = 0;
result = (x >> y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 341: both arguments constants
result = (1073741825.5 >> 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 342: LHS constant
y = 0;
result = (1073741825.5 >> y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 343: RHS constant
x = 1073741825.5;
result = (x >> 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 344: both arguments variables
x = 1073741825.6;
y = 0;
result = (x >> y);
check = 1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 345: both arguments constants
result = (1073741825.6 >> 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 346: LHS constant
y = 0;
result = (1073741825.6 >> y)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 347: RHS constant
x = 1073741825.6;
result = (x >> 0)
check = 1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 348: both arguments variables
x = 1073741824.6;
y = 0;
result = (x >> y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 349: both arguments constants
result = (1073741824.6 >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 350: LHS constant
y = 0;
result = (1073741824.6 >> y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 351: RHS constant
x = 1073741824.6;
result = (x >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 352: both arguments variables
x = 1073741824.5;
y = 0;
result = (x >> y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 353: both arguments constants
result = (1073741824.5 >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 354: LHS constant
y = 0;
result = (1073741824.5 >> y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 355: RHS constant
x = 1073741824.5;
result = (x >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 356: both arguments variables
x = 1073741824.4;
y = 0;
result = (x >> y);
check = 1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 357: both arguments constants
result = (1073741824.4 >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 358: LHS constant
y = 0;
result = (1073741824.4 >> y)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 359: RHS constant
x = 1073741824.4;
result = (x >> 0)
check = 1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 360: both arguments variables
x = -1073741822.6;
y = 0;
result = (x >> y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 361: both arguments constants
result = (-1073741822.6 >> 0)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 362: LHS constant
y = 0;
result = (-1073741822.6 >> y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 363: RHS constant
x = -1073741822.6;
result = (x >> 0)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 364: both arguments variables
x = -1073741822.5;
y = 0;
result = (x >> y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 365: both arguments constants
result = (-1073741822.5 >> 0)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 366: LHS constant
y = 0;
result = (-1073741822.5 >> y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 367: RHS constant
x = -1073741822.5;
result = (x >> 0)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 368: both arguments variables
x = -1073741822.4;
y = 0;
result = (x >> y);
check = -1073741822;
if(result != check) { fail(test, check, result); } ++test; 

// Test 369: both arguments constants
result = (-1073741822.4 >> 0)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 370: LHS constant
y = 0;
result = (-1073741822.4 >> y)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 371: RHS constant
x = -1073741822.4;
result = (x >> 0)
check = -1073741822
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 372: both arguments variables
x = -1073741823.4;
y = 0;
result = (x >> y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 373: both arguments constants
result = (-1073741823.4 >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 374: LHS constant
y = 0;
result = (-1073741823.4 >> y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 375: RHS constant
x = -1073741823.4;
result = (x >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 376: both arguments variables
x = -1073741823.5;
y = 0;
result = (x >> y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 377: both arguments constants
result = (-1073741823.5 >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 378: LHS constant
y = 0;
result = (-1073741823.5 >> y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 379: RHS constant
x = -1073741823.5;
result = (x >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 380: both arguments variables
x = -1073741823.6;
y = 0;
result = (x >> y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 381: both arguments constants
result = (-1073741823.6 >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 382: LHS constant
y = 0;
result = (-1073741823.6 >> y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 383: RHS constant
x = -1073741823.6;
result = (x >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 384: both arguments variables
x = -1073741823.6;
y = 0;
result = (x >> y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 385: both arguments constants
result = (-1073741823.6 >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 386: LHS constant
y = 0;
result = (-1073741823.6 >> y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 387: RHS constant
x = -1073741823.6;
result = (x >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 388: both arguments variables
x = -1073741823.5;
y = 0;
result = (x >> y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 389: both arguments constants
result = (-1073741823.5 >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 390: LHS constant
y = 0;
result = (-1073741823.5 >> y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 391: RHS constant
x = -1073741823.5;
result = (x >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 392: both arguments variables
x = -1073741823.4;
y = 0;
result = (x >> y);
check = -1073741823;
if(result != check) { fail(test, check, result); } ++test; 

// Test 393: both arguments constants
result = (-1073741823.4 >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 394: LHS constant
y = 0;
result = (-1073741823.4 >> y)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 395: RHS constant
x = -1073741823.4;
result = (x >> 0)
check = -1073741823
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 396: both arguments variables
x = -1073741824.4;
y = 0;
result = (x >> y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 397: both arguments constants
result = (-1073741824.4 >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 398: LHS constant
y = 0;
result = (-1073741824.4 >> y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 399: RHS constant
x = -1073741824.4;
result = (x >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test4() {
var x;
var y;
var result;
var check;
// Test 400: both arguments variables
x = -1073741824.5;
y = 0;
result = (x >> y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 401: both arguments constants
result = (-1073741824.5 >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 402: LHS constant
y = 0;
result = (-1073741824.5 >> y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 403: RHS constant
x = -1073741824.5;
result = (x >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 404: both arguments variables
x = -1073741824.6;
y = 0;
result = (x >> y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 405: both arguments constants
result = (-1073741824.6 >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 406: LHS constant
y = 0;
result = (-1073741824.6 >> y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 407: RHS constant
x = -1073741824.6;
result = (x >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 408: both arguments variables
x = -1073741824.6;
y = 0;
result = (x >> y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 409: both arguments constants
result = (-1073741824.6 >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 410: LHS constant
y = 0;
result = (-1073741824.6 >> y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 411: RHS constant
x = -1073741824.6;
result = (x >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 412: both arguments variables
x = -1073741824.5;
y = 0;
result = (x >> y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 413: both arguments constants
result = (-1073741824.5 >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 414: LHS constant
y = 0;
result = (-1073741824.5 >> y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 415: RHS constant
x = -1073741824.5;
result = (x >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 416: both arguments variables
x = -1073741824.4;
y = 0;
result = (x >> y);
check = -1073741824;
if(result != check) { fail(test, check, result); } ++test; 

// Test 417: both arguments constants
result = (-1073741824.4 >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 418: LHS constant
y = 0;
result = (-1073741824.4 >> y)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 419: RHS constant
x = -1073741824.4;
result = (x >> 0)
check = -1073741824
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 420: both arguments variables
x = -1073741825.4;
y = 0;
result = (x >> y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 421: both arguments constants
result = (-1073741825.4 >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 422: LHS constant
y = 0;
result = (-1073741825.4 >> y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 423: RHS constant
x = -1073741825.4;
result = (x >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 424: both arguments variables
x = -1073741825.5;
y = 0;
result = (x >> y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 425: both arguments constants
result = (-1073741825.5 >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 426: LHS constant
y = 0;
result = (-1073741825.5 >> y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 427: RHS constant
x = -1073741825.5;
result = (x >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 428: both arguments variables
x = -1073741825.6;
y = 0;
result = (x >> y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 429: both arguments constants
result = (-1073741825.6 >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 430: LHS constant
y = 0;
result = (-1073741825.6 >> y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 431: RHS constant
x = -1073741825.6;
result = (x >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 432: both arguments variables
x = -1073741825.6;
y = 0;
result = (x >> y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 433: both arguments constants
result = (-1073741825.6 >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 434: LHS constant
y = 0;
result = (-1073741825.6 >> y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 435: RHS constant
x = -1073741825.6;
result = (x >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 436: both arguments variables
x = -1073741825.5;
y = 0;
result = (x >> y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 437: both arguments constants
result = (-1073741825.5 >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 438: LHS constant
y = 0;
result = (-1073741825.5 >> y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 439: RHS constant
x = -1073741825.5;
result = (x >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 440: both arguments variables
x = -1073741825.4;
y = 0;
result = (x >> y);
check = -1073741825;
if(result != check) { fail(test, check, result); } ++test; 

// Test 441: both arguments constants
result = (-1073741825.4 >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 442: LHS constant
y = 0;
result = (-1073741825.4 >> y)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 443: RHS constant
x = -1073741825.4;
result = (x >> 0)
check = -1073741825
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 444: both arguments variables
x = -1073741826.4;
y = 0;
result = (x >> y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 445: both arguments constants
result = (-1073741826.4 >> 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 446: LHS constant
y = 0;
result = (-1073741826.4 >> y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 447: RHS constant
x = -1073741826.4;
result = (x >> 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 448: both arguments variables
x = -1073741826.5;
y = 0;
result = (x >> y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 449: both arguments constants
result = (-1073741826.5 >> 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 450: LHS constant
y = 0;
result = (-1073741826.5 >> y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 451: RHS constant
x = -1073741826.5;
result = (x >> 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 452: both arguments variables
x = -1073741826.6;
y = 0;
result = (x >> y);
check = -1073741826;
if(result != check) { fail(test, check, result); } ++test; 

// Test 453: both arguments constants
result = (-1073741826.6 >> 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 454: LHS constant
y = 0;
result = (-1073741826.6 >> y)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 455: RHS constant
x = -1073741826.6;
result = (x >> 0)
check = -1073741826
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 456: both arguments variables
x = 2147483646.4;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 457: both arguments constants
result = (2147483646.4 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 458: LHS constant
y = 0;
result = (2147483646.4 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 459: RHS constant
x = 2147483646.4;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 460: both arguments variables
x = 2147483646.5;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 461: both arguments constants
result = (2147483646.5 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 462: LHS constant
y = 0;
result = (2147483646.5 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 463: RHS constant
x = 2147483646.5;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 464: both arguments variables
x = 2147483646.6;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 465: both arguments constants
result = (2147483646.6 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 466: LHS constant
y = 0;
result = (2147483646.6 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 467: RHS constant
x = 2147483646.6;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 468: both arguments variables
x = 2147483645.6;
y = 0;
result = (x >> y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 469: both arguments constants
result = (2147483645.6 >> 0)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 470: LHS constant
y = 0;
result = (2147483645.6 >> y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 471: RHS constant
x = 2147483645.6;
result = (x >> 0)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 472: both arguments variables
x = 2147483645.5;
y = 0;
result = (x >> y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 473: both arguments constants
result = (2147483645.5 >> 0)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 474: LHS constant
y = 0;
result = (2147483645.5 >> y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 475: RHS constant
x = 2147483645.5;
result = (x >> 0)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 476: both arguments variables
x = 2147483645.4;
y = 0;
result = (x >> y);
check = 2147483645;
if(result != check) { fail(test, check, result); } ++test; 

// Test 477: both arguments constants
result = (2147483645.4 >> 0)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 478: LHS constant
y = 0;
result = (2147483645.4 >> y)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 479: RHS constant
x = 2147483645.4;
result = (x >> 0)
check = 2147483645
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 480: both arguments variables
x = 2147483647.4;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 481: both arguments constants
result = (2147483647.4 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 482: LHS constant
y = 0;
result = (2147483647.4 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 483: RHS constant
x = 2147483647.4;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 484: both arguments variables
x = 2147483647.5;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 485: both arguments constants
result = (2147483647.5 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 486: LHS constant
y = 0;
result = (2147483647.5 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 487: RHS constant
x = 2147483647.5;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 488: both arguments variables
x = 2147483647.6;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 489: both arguments constants
result = (2147483647.6 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 490: LHS constant
y = 0;
result = (2147483647.6 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 491: RHS constant
x = 2147483647.6;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 492: both arguments variables
x = 2147483646.6;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 493: both arguments constants
result = (2147483646.6 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 494: LHS constant
y = 0;
result = (2147483646.6 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 495: RHS constant
x = 2147483646.6;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 496: both arguments variables
x = 2147483646.5;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 497: both arguments constants
result = (2147483646.5 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 498: LHS constant
y = 0;
result = (2147483646.5 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 499: RHS constant
x = 2147483646.5;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test5() {
var x;
var y;
var result;
var check;
// Test 500: both arguments variables
x = 2147483646.4;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 501: both arguments constants
result = (2147483646.4 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 502: LHS constant
y = 0;
result = (2147483646.4 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 503: RHS constant
x = 2147483646.4;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 504: both arguments variables
x = 2147483648.4;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 505: both arguments constants
result = (2147483648.4 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 506: LHS constant
y = 0;
result = (2147483648.4 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 507: RHS constant
x = 2147483648.4;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 508: both arguments variables
x = 2147483648.5;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 509: both arguments constants
result = (2147483648.5 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 510: LHS constant
y = 0;
result = (2147483648.5 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 511: RHS constant
x = 2147483648.5;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 512: both arguments variables
x = 2147483648.6;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 513: both arguments constants
result = (2147483648.6 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 514: LHS constant
y = 0;
result = (2147483648.6 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 515: RHS constant
x = 2147483648.6;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 516: both arguments variables
x = 2147483647.6;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 517: both arguments constants
result = (2147483647.6 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 518: LHS constant
y = 0;
result = (2147483647.6 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 519: RHS constant
x = 2147483647.6;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 520: both arguments variables
x = 2147483647.5;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 521: both arguments constants
result = (2147483647.5 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 522: LHS constant
y = 0;
result = (2147483647.5 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 523: RHS constant
x = 2147483647.5;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 524: both arguments variables
x = 2147483647.4;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 525: both arguments constants
result = (2147483647.4 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 526: LHS constant
y = 0;
result = (2147483647.4 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 527: RHS constant
x = 2147483647.4;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 528: both arguments variables
x = 2147483649.4;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 529: both arguments constants
result = (2147483649.4 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 530: LHS constant
y = 0;
result = (2147483649.4 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 531: RHS constant
x = 2147483649.4;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 532: both arguments variables
x = 2147483649.5;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 533: both arguments constants
result = (2147483649.5 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 534: LHS constant
y = 0;
result = (2147483649.5 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 535: RHS constant
x = 2147483649.5;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 536: both arguments variables
x = 2147483649.6;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 537: both arguments constants
result = (2147483649.6 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 538: LHS constant
y = 0;
result = (2147483649.6 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 539: RHS constant
x = 2147483649.6;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 540: both arguments variables
x = 2147483648.6;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 541: both arguments constants
result = (2147483648.6 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 542: LHS constant
y = 0;
result = (2147483648.6 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 543: RHS constant
x = 2147483648.6;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 544: both arguments variables
x = 2147483648.5;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 545: both arguments constants
result = (2147483648.5 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 546: LHS constant
y = 0;
result = (2147483648.5 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 547: RHS constant
x = 2147483648.5;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 548: both arguments variables
x = 2147483648.4;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 549: both arguments constants
result = (2147483648.4 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 550: LHS constant
y = 0;
result = (2147483648.4 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 551: RHS constant
x = 2147483648.4;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 552: both arguments variables
x = -2147483646.6;
y = 0;
result = (x >> y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 553: both arguments constants
result = (-2147483646.6 >> 0)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 554: LHS constant
y = 0;
result = (-2147483646.6 >> y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 555: RHS constant
x = -2147483646.6;
result = (x >> 0)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 556: both arguments variables
x = -2147483646.5;
y = 0;
result = (x >> y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 557: both arguments constants
result = (-2147483646.5 >> 0)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 558: LHS constant
y = 0;
result = (-2147483646.5 >> y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 559: RHS constant
x = -2147483646.5;
result = (x >> 0)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 560: both arguments variables
x = -2147483646.4;
y = 0;
result = (x >> y);
check = -2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 561: both arguments constants
result = (-2147483646.4 >> 0)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 562: LHS constant
y = 0;
result = (-2147483646.4 >> y)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 563: RHS constant
x = -2147483646.4;
result = (x >> 0)
check = -2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 564: both arguments variables
x = -2147483647.4;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 565: both arguments constants
result = (-2147483647.4 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 566: LHS constant
y = 0;
result = (-2147483647.4 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 567: RHS constant
x = -2147483647.4;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 568: both arguments variables
x = -2147483647.5;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 569: both arguments constants
result = (-2147483647.5 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 570: LHS constant
y = 0;
result = (-2147483647.5 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 571: RHS constant
x = -2147483647.5;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 572: both arguments variables
x = -2147483647.6;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 573: both arguments constants
result = (-2147483647.6 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 574: LHS constant
y = 0;
result = (-2147483647.6 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 575: RHS constant
x = -2147483647.6;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 576: both arguments variables
x = -2147483647.6;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 577: both arguments constants
result = (-2147483647.6 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 578: LHS constant
y = 0;
result = (-2147483647.6 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 579: RHS constant
x = -2147483647.6;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 580: both arguments variables
x = -2147483647.5;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 581: both arguments constants
result = (-2147483647.5 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 582: LHS constant
y = 0;
result = (-2147483647.5 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 583: RHS constant
x = -2147483647.5;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 584: both arguments variables
x = -2147483647.4;
y = 0;
result = (x >> y);
check = -2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 585: both arguments constants
result = (-2147483647.4 >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 586: LHS constant
y = 0;
result = (-2147483647.4 >> y)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 587: RHS constant
x = -2147483647.4;
result = (x >> 0)
check = -2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 588: both arguments variables
x = -2147483648.4;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 589: both arguments constants
result = (-2147483648.4 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 590: LHS constant
y = 0;
result = (-2147483648.4 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 591: RHS constant
x = -2147483648.4;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 592: both arguments variables
x = -2147483648.5;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 593: both arguments constants
result = (-2147483648.5 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 594: LHS constant
y = 0;
result = (-2147483648.5 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 595: RHS constant
x = -2147483648.5;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 596: both arguments variables
x = -2147483648.6;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 597: both arguments constants
result = (-2147483648.6 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 598: LHS constant
y = 0;
result = (-2147483648.6 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 599: RHS constant
x = -2147483648.6;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test6() {
var x;
var y;
var result;
var check;
// Test 600: both arguments variables
x = -2147483648.6;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 601: both arguments constants
result = (-2147483648.6 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 602: LHS constant
y = 0;
result = (-2147483648.6 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 603: RHS constant
x = -2147483648.6;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 604: both arguments variables
x = -2147483648.5;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 605: both arguments constants
result = (-2147483648.5 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 606: LHS constant
y = 0;
result = (-2147483648.5 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 607: RHS constant
x = -2147483648.5;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 608: both arguments variables
x = -2147483648.4;
y = 0;
result = (x >> y);
check = -2147483648;
if(result != check) { fail(test, check, result); } ++test; 

// Test 609: both arguments constants
result = (-2147483648.4 >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 610: LHS constant
y = 0;
result = (-2147483648.4 >> y)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 611: RHS constant
x = -2147483648.4;
result = (x >> 0)
check = -2147483648
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 612: both arguments variables
x = -2147483649.4;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 613: both arguments constants
result = (-2147483649.4 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 614: LHS constant
y = 0;
result = (-2147483649.4 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 615: RHS constant
x = -2147483649.4;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 616: both arguments variables
x = -2147483649.5;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 617: both arguments constants
result = (-2147483649.5 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 618: LHS constant
y = 0;
result = (-2147483649.5 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 619: RHS constant
x = -2147483649.5;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 620: both arguments variables
x = -2147483649.6;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 621: both arguments constants
result = (-2147483649.6 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 622: LHS constant
y = 0;
result = (-2147483649.6 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 623: RHS constant
x = -2147483649.6;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 624: both arguments variables
x = -2147483649.6;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 625: both arguments constants
result = (-2147483649.6 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 626: LHS constant
y = 0;
result = (-2147483649.6 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 627: RHS constant
x = -2147483649.6;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 628: both arguments variables
x = -2147483649.5;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 629: both arguments constants
result = (-2147483649.5 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 630: LHS constant
y = 0;
result = (-2147483649.5 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 631: RHS constant
x = -2147483649.5;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 632: both arguments variables
x = -2147483649.4;
y = 0;
result = (x >> y);
check = 2147483647;
if(result != check) { fail(test, check, result); } ++test; 

// Test 633: both arguments constants
result = (-2147483649.4 >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 634: LHS constant
y = 0;
result = (-2147483649.4 >> y)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 635: RHS constant
x = -2147483649.4;
result = (x >> 0)
check = 2147483647
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 636: both arguments variables
x = -2147483650.4;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 637: both arguments constants
result = (-2147483650.4 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 638: LHS constant
y = 0;
result = (-2147483650.4 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 639: RHS constant
x = -2147483650.4;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 640: both arguments variables
x = -2147483650.5;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 641: both arguments constants
result = (-2147483650.5 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 642: LHS constant
y = 0;
result = (-2147483650.5 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 643: RHS constant
x = -2147483650.5;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 644: both arguments variables
x = -2147483650.6;
y = 0;
result = (x >> y);
check = 2147483646;
if(result != check) { fail(test, check, result); } ++test; 

// Test 645: both arguments constants
result = (-2147483650.6 >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 646: LHS constant
y = 0;
result = (-2147483650.6 >> y)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 647: RHS constant
x = -2147483650.6;
result = (x >> 0)
check = 2147483646
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 648: both arguments variables
x = 4294967295.4;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 649: both arguments constants
result = (4294967295.4 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 650: LHS constant
y = 0;
result = (4294967295.4 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 651: RHS constant
x = 4294967295.4;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 652: both arguments variables
x = 4294967295.5;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 653: both arguments constants
result = (4294967295.5 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 654: LHS constant
y = 0;
result = (4294967295.5 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 655: RHS constant
x = 4294967295.5;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 656: both arguments variables
x = 4294967295.6;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 657: both arguments constants
result = (4294967295.6 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 658: LHS constant
y = 0;
result = (4294967295.6 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 659: RHS constant
x = 4294967295.6;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 660: both arguments variables
x = 4294967294.6;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 661: both arguments constants
result = (4294967294.6 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 662: LHS constant
y = 0;
result = (4294967294.6 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 663: RHS constant
x = 4294967294.6;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 664: both arguments variables
x = 4294967294.5;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 665: both arguments constants
result = (4294967294.5 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 666: LHS constant
y = 0;
result = (4294967294.5 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 667: RHS constant
x = 4294967294.5;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 668: both arguments variables
x = 4294967294.4;
y = 0;
result = (x >> y);
check = -2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 669: both arguments constants
result = (4294967294.4 >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 670: LHS constant
y = 0;
result = (4294967294.4 >> y)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 671: RHS constant
x = 4294967294.4;
result = (x >> 0)
check = -2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 672: both arguments variables
x = 4294967296.4;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 673: both arguments constants
result = (4294967296.4 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 674: LHS constant
y = 0;
result = (4294967296.4 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 675: RHS constant
x = 4294967296.4;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 676: both arguments variables
x = 4294967296.5;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 677: both arguments constants
result = (4294967296.5 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 678: LHS constant
y = 0;
result = (4294967296.5 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 679: RHS constant
x = 4294967296.5;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 680: both arguments variables
x = 4294967296.6;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 681: both arguments constants
result = (4294967296.6 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 682: LHS constant
y = 0;
result = (4294967296.6 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 683: RHS constant
x = 4294967296.6;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 684: both arguments variables
x = 4294967295.6;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 685: both arguments constants
result = (4294967295.6 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 686: LHS constant
y = 0;
result = (4294967295.6 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 687: RHS constant
x = 4294967295.6;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 688: both arguments variables
x = 4294967295.5;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 689: both arguments constants
result = (4294967295.5 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 690: LHS constant
y = 0;
result = (4294967295.5 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 691: RHS constant
x = 4294967295.5;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 692: both arguments variables
x = 4294967295.4;
y = 0;
result = (x >> y);
check = -1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 693: both arguments constants
result = (4294967295.4 >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 694: LHS constant
y = 0;
result = (4294967295.4 >> y)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 695: RHS constant
x = 4294967295.4;
result = (x >> 0)
check = -1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 696: both arguments variables
x = -4294967294.6;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 697: both arguments constants
result = (-4294967294.6 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 698: LHS constant
y = 0;
result = (-4294967294.6 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 699: RHS constant
x = -4294967294.6;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

}
function test7() {
var x;
var y;
var result;
var check;
// Test 700: both arguments variables
x = -4294967294.5;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 701: both arguments constants
result = (-4294967294.5 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 702: LHS constant
y = 0;
result = (-4294967294.5 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 703: RHS constant
x = -4294967294.5;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 704: both arguments variables
x = -4294967294.4;
y = 0;
result = (x >> y);
check = 2;
if(result != check) { fail(test, check, result); } ++test; 

// Test 705: both arguments constants
result = (-4294967294.4 >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 706: LHS constant
y = 0;
result = (-4294967294.4 >> y)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 707: RHS constant
x = -4294967294.4;
result = (x >> 0)
check = 2
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 708: both arguments variables
x = -4294967295.4;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 709: both arguments constants
result = (-4294967295.4 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 710: LHS constant
y = 0;
result = (-4294967295.4 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 711: RHS constant
x = -4294967295.4;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 712: both arguments variables
x = -4294967295.5;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 713: both arguments constants
result = (-4294967295.5 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 714: LHS constant
y = 0;
result = (-4294967295.5 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 715: RHS constant
x = -4294967295.5;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 716: both arguments variables
x = -4294967295.6;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 717: both arguments constants
result = (-4294967295.6 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 718: LHS constant
y = 0;
result = (-4294967295.6 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 719: RHS constant
x = -4294967295.6;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 720: both arguments variables
x = -4294967295.6;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 721: both arguments constants
result = (-4294967295.6 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 722: LHS constant
y = 0;
result = (-4294967295.6 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 723: RHS constant
x = -4294967295.6;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 724: both arguments variables
x = -4294967295.5;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 725: both arguments constants
result = (-4294967295.5 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 726: LHS constant
y = 0;
result = (-4294967295.5 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 727: RHS constant
x = -4294967295.5;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 728: both arguments variables
x = -4294967295.4;
y = 0;
result = (x >> y);
check = 1;
if(result != check) { fail(test, check, result); } ++test; 

// Test 729: both arguments constants
result = (-4294967295.4 >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 730: LHS constant
y = 0;
result = (-4294967295.4 >> y)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 731: RHS constant
x = -4294967295.4;
result = (x >> 0)
check = 1
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 732: both arguments variables
x = -4294967296.4;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 733: both arguments constants
result = (-4294967296.4 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 734: LHS constant
y = 0;
result = (-4294967296.4 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 735: RHS constant
x = -4294967296.4;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 736: both arguments variables
x = -4294967296.5;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 737: both arguments constants
result = (-4294967296.5 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 738: LHS constant
y = 0;
result = (-4294967296.5 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 739: RHS constant
x = -4294967296.5;
result = (x >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 740: both arguments variables
x = -4294967296.6;
y = 0;
result = (x >> y);
check = 0;
if(result != check) { fail(test, check, result); } ++test; 

// Test 741: both arguments constants
result = (-4294967296.6 >> 0)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 742: LHS constant
y = 0;
result = (-4294967296.6 >> y)
check = 0
if(result != check) {{ fail(test, check, result); }} ++test; 

// Test 743: RHS constant
x = -4294967296.6;
result = (x >> 0)
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
WScript.Echo("done");
