//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var test = 1; 
function fail(n, expected, result) { WScript.Echo("failure in test " + test + "; expected " + check + ", got " + result); }
function test0() {
var x;
var y;
var result;
var check;
// Test0
WScript.Echo('begin test 0');
for(var i = 0; i < 9; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test1
WScript.Echo('begin test 1');
for(var i = 0; i < 9; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test2
WScript.Echo('begin test 2');
for(var i = 0; i > -9; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test3
WScript.Echo('begin test 3');
for(var i = 0; i > -9; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test4
WScript.Echo('begin test 4');
for(var i = 0; i <= 9; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test1() {
var x;
var y;
var result;
var check;
// Test5
WScript.Echo('begin test 5');
for(var i = 0; i <= 9; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test6
WScript.Echo('begin test 6');
for(var i = 0; i >= -9; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test7
WScript.Echo('begin test 7');
for(var i = 0; i >= -9; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test8
WScript.Echo('begin test 8');
for(var i = 0; i != 9; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test9
WScript.Echo('begin test 9');
for(var i = 0; i != 9; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test2() {
var x;
var y;
var result;
var check;
// Test10
WScript.Echo('begin test 10');
for(var i = 0; i != -9; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test11
WScript.Echo('begin test 11');
for(var i = 0; i != -9; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test12
WScript.Echo('begin test 12');
for(var i = 0; i < 9; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test13
WScript.Echo('begin test 13');
for(var i = 0; i > -9; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test14
WScript.Echo('begin test 14');
for(var i = 0; i <= 9; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test3() {
var x;
var y;
var result;
var check;
// Test15
WScript.Echo('begin test 15');
for(var i = 0; i >= -9; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test16
WScript.Echo('begin test 16');
for(var i = 0; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test17
WScript.Echo('begin test 17');
for(var i = 1; i < 10; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test18
WScript.Echo('begin test 18');
for(var i = 1; i < 10; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test19
WScript.Echo('begin test 19');
for(var i = 1; i > -8; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test4() {
var x;
var y;
var result;
var check;
// Test20
WScript.Echo('begin test 20');
for(var i = 1; i > -8; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test21
WScript.Echo('begin test 21');
for(var i = 1; i <= 10; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test22
WScript.Echo('begin test 22');
for(var i = 1; i <= 10; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test23
WScript.Echo('begin test 23');
for(var i = 1; i >= -8; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test24
WScript.Echo('begin test 24');
for(var i = 1; i >= -8; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test5() {
var x;
var y;
var result;
var check;
// Test25
WScript.Echo('begin test 25');
for(var i = 1; i != 10; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test26
WScript.Echo('begin test 26');
for(var i = 1; i != 10; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test27
WScript.Echo('begin test 27');
for(var i = 1; i != -8; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test28
WScript.Echo('begin test 28');
for(var i = 1; i != -8; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test29
WScript.Echo('begin test 29');
for(var i = 1; i < 10; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test6() {
var x;
var y;
var result;
var check;
// Test30
WScript.Echo('begin test 30');
for(var i = 1; i > -8; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test31
WScript.Echo('begin test 31');
for(var i = 1; i <= 10; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test32
WScript.Echo('begin test 32');
for(var i = 1; i >= -8; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test33
WScript.Echo('begin test 33');
for(var i = 1; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test34
WScript.Echo('begin test 34');
for(var i = 1; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test7() {
var x;
var y;
var result;
var check;
// Test35
WScript.Echo('begin test 35');
for(var i = 1; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test36
WScript.Echo('begin test 36');
for(var i = 1; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test37
WScript.Echo('begin test 37');
for(var i = 1; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test38
WScript.Echo('begin test 38');
for(var i = -1; i < 8; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test39
WScript.Echo('begin test 39');
for(var i = -1; i < 8; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test8() {
var x;
var y;
var result;
var check;
// Test40
WScript.Echo('begin test 40');
for(var i = -1; i > -10; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test41
WScript.Echo('begin test 41');
for(var i = -1; i > -10; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test42
WScript.Echo('begin test 42');
for(var i = -1; i <= 8; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test43
WScript.Echo('begin test 43');
for(var i = -1; i <= 8; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test44
WScript.Echo('begin test 44');
for(var i = -1; i >= -10; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test9() {
var x;
var y;
var result;
var check;
// Test45
WScript.Echo('begin test 45');
for(var i = -1; i >= -10; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test46
WScript.Echo('begin test 46');
for(var i = -1; i != 8; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test47
WScript.Echo('begin test 47');
for(var i = -1; i != 8; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test48
WScript.Echo('begin test 48');
for(var i = -1; i != -10; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test49
WScript.Echo('begin test 49');
for(var i = -1; i != -10; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test10() {
var x;
var y;
var result;
var check;
// Test50
WScript.Echo('begin test 50');
for(var i = -1; i < 8; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test51
WScript.Echo('begin test 51');
for(var i = -1; i > -10; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test52
WScript.Echo('begin test 52');
for(var i = -1; i <= 8; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test53
WScript.Echo('begin test 53');
for(var i = -1; i >= -10; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test54
WScript.Echo('begin test 54');
for(var i = -1; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test11() {
var x;
var y;
var result;
var check;
// Test55
WScript.Echo('begin test 55');
for(var i = 2; i < 11; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test56
WScript.Echo('begin test 56');
for(var i = 2; i < 11; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test57
WScript.Echo('begin test 57');
for(var i = 2; i > -7; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test58
WScript.Echo('begin test 58');
for(var i = 2; i > -7; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test59
WScript.Echo('begin test 59');
for(var i = 2; i <= 11; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test12() {
var x;
var y;
var result;
var check;
// Test60
WScript.Echo('begin test 60');
for(var i = 2; i <= 11; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test61
WScript.Echo('begin test 61');
for(var i = 2; i >= -7; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test62
WScript.Echo('begin test 62');
for(var i = 2; i >= -7; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test63
WScript.Echo('begin test 63');
for(var i = 2; i != 11; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test64
WScript.Echo('begin test 64');
for(var i = 2; i != 11; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test13() {
var x;
var y;
var result;
var check;
// Test65
WScript.Echo('begin test 65');
for(var i = 2; i != -7; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test66
WScript.Echo('begin test 66');
for(var i = 2; i != -7; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test67
WScript.Echo('begin test 67');
for(var i = 2; i < 11; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test68
WScript.Echo('begin test 68');
for(var i = 2; i > -7; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test69
WScript.Echo('begin test 69');
for(var i = 2; i <= 11; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test14() {
var x;
var y;
var result;
var check;
// Test70
WScript.Echo('begin test 70');
for(var i = 2; i >= -7; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test71
WScript.Echo('begin test 71');
for(var i = 2; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test72
WScript.Echo('begin test 72');
for(var i = 2; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test73
WScript.Echo('begin test 73');
for(var i = 2; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test74
WScript.Echo('begin test 74');
for(var i = 2; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test15() {
var x;
var y;
var result;
var check;
// Test75
WScript.Echo('begin test 75');
for(var i = 2; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test76
WScript.Echo('begin test 76');
for(var i = -2; i < 7; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test77
WScript.Echo('begin test 77');
for(var i = -2; i < 7; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test78
WScript.Echo('begin test 78');
for(var i = -2; i > -11; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test79
WScript.Echo('begin test 79');
for(var i = -2; i > -11; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test16() {
var x;
var y;
var result;
var check;
// Test80
WScript.Echo('begin test 80');
for(var i = -2; i <= 7; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test81
WScript.Echo('begin test 81');
for(var i = -2; i <= 7; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test82
WScript.Echo('begin test 82');
for(var i = -2; i >= -11; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test83
WScript.Echo('begin test 83');
for(var i = -2; i >= -11; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test84
WScript.Echo('begin test 84');
for(var i = -2; i != 7; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test17() {
var x;
var y;
var result;
var check;
// Test85
WScript.Echo('begin test 85');
for(var i = -2; i != 7; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test86
WScript.Echo('begin test 86');
for(var i = -2; i != -11; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test87
WScript.Echo('begin test 87');
for(var i = -2; i != -11; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test88
WScript.Echo('begin test 88');
for(var i = -2; i < 7; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test89
WScript.Echo('begin test 89');
for(var i = -2; i > -11; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test18() {
var x;
var y;
var result;
var check;
// Test90
WScript.Echo('begin test 90');
for(var i = -2; i <= 7; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test91
WScript.Echo('begin test 91');
for(var i = -2; i >= -11; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test92
WScript.Echo('begin test 92');
for(var i = -2; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test93
WScript.Echo('begin test 93');
for(var i = 3; i < 12; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test94
WScript.Echo('begin test 94');
for(var i = 3; i < 12; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test19() {
var x;
var y;
var result;
var check;
// Test95
WScript.Echo('begin test 95');
for(var i = 3; i > -6; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test96
WScript.Echo('begin test 96');
for(var i = 3; i > -6; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test97
WScript.Echo('begin test 97');
for(var i = 3; i <= 12; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test98
WScript.Echo('begin test 98');
for(var i = 3; i <= 12; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test99
WScript.Echo('begin test 99');
for(var i = 3; i >= -6; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test20() {
var x;
var y;
var result;
var check;
// Test100
WScript.Echo('begin test 100');
for(var i = 3; i >= -6; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test101
WScript.Echo('begin test 101');
for(var i = 3; i != 12; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test102
WScript.Echo('begin test 102');
for(var i = 3; i != 12; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test103
WScript.Echo('begin test 103');
for(var i = 3; i != -6; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test104
WScript.Echo('begin test 104');
for(var i = 3; i != -6; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test21() {
var x;
var y;
var result;
var check;
// Test105
WScript.Echo('begin test 105');
for(var i = 3; i < 12; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test106
WScript.Echo('begin test 106');
for(var i = 3; i > -6; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test107
WScript.Echo('begin test 107');
for(var i = 3; i <= 12; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test108
WScript.Echo('begin test 108');
for(var i = 3; i >= -6; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test109
WScript.Echo('begin test 109');
for(var i = 3; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test22() {
var x;
var y;
var result;
var check;
// Test110
WScript.Echo('begin test 110');
for(var i = 3; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test111
WScript.Echo('begin test 111');
for(var i = 3; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test112
WScript.Echo('begin test 112');
for(var i = 3; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test113
WScript.Echo('begin test 113');
for(var i = 3; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test114
WScript.Echo('begin test 114');
for(var i = -3; i < 6; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test23() {
var x;
var y;
var result;
var check;
// Test115
WScript.Echo('begin test 115');
for(var i = -3; i < 6; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test116
WScript.Echo('begin test 116');
for(var i = -3; i > -12; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test117
WScript.Echo('begin test 117');
for(var i = -3; i > -12; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test118
WScript.Echo('begin test 118');
for(var i = -3; i <= 6; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test119
WScript.Echo('begin test 119');
for(var i = -3; i <= 6; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test24() {
var x;
var y;
var result;
var check;
// Test120
WScript.Echo('begin test 120');
for(var i = -3; i >= -12; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test121
WScript.Echo('begin test 121');
for(var i = -3; i >= -12; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test122
WScript.Echo('begin test 122');
for(var i = -3; i != 6; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test123
WScript.Echo('begin test 123');
for(var i = -3; i != 6; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test124
WScript.Echo('begin test 124');
for(var i = -3; i != -12; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test25() {
var x;
var y;
var result;
var check;
// Test125
WScript.Echo('begin test 125');
for(var i = -3; i != -12; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test126
WScript.Echo('begin test 126');
for(var i = -3; i < 6; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test127
WScript.Echo('begin test 127');
for(var i = -3; i > -12; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test128
WScript.Echo('begin test 128');
for(var i = -3; i <= 6; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test129
WScript.Echo('begin test 129');
for(var i = -3; i >= -12; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test26() {
var x;
var y;
var result;
var check;
// Test130
WScript.Echo('begin test 130');
for(var i = -3; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test131
WScript.Echo('begin test 131');
for(var i = 4; i < 13; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test132
WScript.Echo('begin test 132');
for(var i = 4; i < 13; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test133
WScript.Echo('begin test 133');
for(var i = 4; i > -5; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test134
WScript.Echo('begin test 134');
for(var i = 4; i > -5; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test27() {
var x;
var y;
var result;
var check;
// Test135
WScript.Echo('begin test 135');
for(var i = 4; i <= 13; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test136
WScript.Echo('begin test 136');
for(var i = 4; i <= 13; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test137
WScript.Echo('begin test 137');
for(var i = 4; i >= -5; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test138
WScript.Echo('begin test 138');
for(var i = 4; i >= -5; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test139
WScript.Echo('begin test 139');
for(var i = 4; i != 13; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test28() {
var x;
var y;
var result;
var check;
// Test140
WScript.Echo('begin test 140');
for(var i = 4; i != 13; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test141
WScript.Echo('begin test 141');
for(var i = 4; i != -5; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test142
WScript.Echo('begin test 142');
for(var i = 4; i != -5; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test143
WScript.Echo('begin test 143');
for(var i = 4; i < 13; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test144
WScript.Echo('begin test 144');
for(var i = 4; i > -5; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test29() {
var x;
var y;
var result;
var check;
// Test145
WScript.Echo('begin test 145');
for(var i = 4; i <= 13; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test146
WScript.Echo('begin test 146');
for(var i = 4; i >= -5; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test147
WScript.Echo('begin test 147');
for(var i = 4; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test148
WScript.Echo('begin test 148');
for(var i = 4; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test149
WScript.Echo('begin test 149');
for(var i = 4; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test30() {
var x;
var y;
var result;
var check;
// Test150
WScript.Echo('begin test 150');
for(var i = 4; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test151
WScript.Echo('begin test 151');
for(var i = 4; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test152
WScript.Echo('begin test 152');
for(var i = -4; i < 5; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test153
WScript.Echo('begin test 153');
for(var i = -4; i < 5; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test154
WScript.Echo('begin test 154');
for(var i = -4; i > -13; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test31() {
var x;
var y;
var result;
var check;
// Test155
WScript.Echo('begin test 155');
for(var i = -4; i > -13; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test156
WScript.Echo('begin test 156');
for(var i = -4; i <= 5; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test157
WScript.Echo('begin test 157');
for(var i = -4; i <= 5; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test158
WScript.Echo('begin test 158');
for(var i = -4; i >= -13; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test159
WScript.Echo('begin test 159');
for(var i = -4; i >= -13; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test32() {
var x;
var y;
var result;
var check;
// Test160
WScript.Echo('begin test 160');
for(var i = -4; i != 5; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test161
WScript.Echo('begin test 161');
for(var i = -4; i != 5; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test162
WScript.Echo('begin test 162');
for(var i = -4; i != -13; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test163
WScript.Echo('begin test 163');
for(var i = -4; i != -13; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test164
WScript.Echo('begin test 164');
for(var i = -4; i < 5; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test33() {
var x;
var y;
var result;
var check;
// Test165
WScript.Echo('begin test 165');
for(var i = -4; i > -13; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test166
WScript.Echo('begin test 166');
for(var i = -4; i <= 5; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test167
WScript.Echo('begin test 167');
for(var i = -4; i >= -13; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test168
WScript.Echo('begin test 168');
for(var i = -4; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test169
WScript.Echo('begin test 169');
for(var i = 8; i < 17; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test34() {
var x;
var y;
var result;
var check;
// Test170
WScript.Echo('begin test 170');
for(var i = 8; i < 17; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test171
WScript.Echo('begin test 171');
for(var i = 8; i > -1; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test172
WScript.Echo('begin test 172');
for(var i = 8; i > -1; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test173
WScript.Echo('begin test 173');
for(var i = 8; i <= 17; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test174
WScript.Echo('begin test 174');
for(var i = 8; i <= 17; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test35() {
var x;
var y;
var result;
var check;
// Test175
WScript.Echo('begin test 175');
for(var i = 8; i >= -1; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test176
WScript.Echo('begin test 176');
for(var i = 8; i >= -1; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test177
WScript.Echo('begin test 177');
for(var i = 8; i != 17; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test178
WScript.Echo('begin test 178');
for(var i = 8; i != 17; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test179
WScript.Echo('begin test 179');
for(var i = 8; i != -1; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test36() {
var x;
var y;
var result;
var check;
// Test180
WScript.Echo('begin test 180');
for(var i = 8; i != -1; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test181
WScript.Echo('begin test 181');
for(var i = 8; i < 17; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test182
WScript.Echo('begin test 182');
for(var i = 8; i > -1; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test183
WScript.Echo('begin test 183');
for(var i = 8; i <= 17; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test184
WScript.Echo('begin test 184');
for(var i = 8; i >= -1; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test37() {
var x;
var y;
var result;
var check;
// Test185
WScript.Echo('begin test 185');
for(var i = 8; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test186
WScript.Echo('begin test 186');
for(var i = 8; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test187
WScript.Echo('begin test 187');
for(var i = 8; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test188
WScript.Echo('begin test 188');
for(var i = 8; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test189
WScript.Echo('begin test 189');
for(var i = 8; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test38() {
var x;
var y;
var result;
var check;
// Test190
WScript.Echo('begin test 190');
for(var i = -8; i < 1; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test191
WScript.Echo('begin test 191');
for(var i = -8; i < 1; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test192
WScript.Echo('begin test 192');
for(var i = -8; i > -17; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test193
WScript.Echo('begin test 193');
for(var i = -8; i > -17; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test194
WScript.Echo('begin test 194');
for(var i = -8; i <= 1; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test39() {
var x;
var y;
var result;
var check;
// Test195
WScript.Echo('begin test 195');
for(var i = -8; i <= 1; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test196
WScript.Echo('begin test 196');
for(var i = -8; i >= -17; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test197
WScript.Echo('begin test 197');
for(var i = -8; i >= -17; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test198
WScript.Echo('begin test 198');
for(var i = -8; i != 1; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test199
WScript.Echo('begin test 199');
for(var i = -8; i != 1; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test40() {
var x;
var y;
var result;
var check;
// Test200
WScript.Echo('begin test 200');
for(var i = -8; i != -17; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test201
WScript.Echo('begin test 201');
for(var i = -8; i != -17; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test202
WScript.Echo('begin test 202');
for(var i = -8; i < 1; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test203
WScript.Echo('begin test 203');
for(var i = -8; i > -17; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test204
WScript.Echo('begin test 204');
for(var i = -8; i <= 1; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test41() {
var x;
var y;
var result;
var check;
// Test205
WScript.Echo('begin test 205');
for(var i = -8; i >= -17; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test206
WScript.Echo('begin test 206');
for(var i = -8; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test207
WScript.Echo('begin test 207');
for(var i = 536870911; i < 536870920; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test208
WScript.Echo('begin test 208');
for(var i = 536870911; i < 536870920; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test209
WScript.Echo('begin test 209');
for(var i = 536870911; i > 536870902; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test42() {
var x;
var y;
var result;
var check;
// Test210
WScript.Echo('begin test 210');
for(var i = 536870911; i > 536870902; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test211
WScript.Echo('begin test 211');
for(var i = 536870911; i <= 536870920; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test212
WScript.Echo('begin test 212');
for(var i = 536870911; i <= 536870920; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test213
WScript.Echo('begin test 213');
for(var i = 536870911; i >= 536870902; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test214
WScript.Echo('begin test 214');
for(var i = 536870911; i >= 536870902; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test43() {
var x;
var y;
var result;
var check;
// Test215
WScript.Echo('begin test 215');
for(var i = 536870911; i != 536870920; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test216
WScript.Echo('begin test 216');
for(var i = 536870911; i != 536870920; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test217
WScript.Echo('begin test 217');
for(var i = 536870911; i != 536870902; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test218
WScript.Echo('begin test 218');
for(var i = 536870911; i != 536870902; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test219
WScript.Echo('begin test 219');
for(var i = 536870911; i < 536870920; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test44() {
var x;
var y;
var result;
var check;
// Test220
WScript.Echo('begin test 220');
for(var i = 536870911; i > 536870902; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test221
WScript.Echo('begin test 221');
for(var i = 536870911; i <= 536870920; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test222
WScript.Echo('begin test 222');
for(var i = 536870911; i >= 536870902; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test223
WScript.Echo('begin test 223');
for(var i = 536870911; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test224
WScript.Echo('begin test 224');
for(var i = 536870911; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test45() {
var x;
var y;
var result;
var check;
// Test225
WScript.Echo('begin test 225');
for(var i = 536870911; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test226
WScript.Echo('begin test 226');
for(var i = 536870911; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test227
WScript.Echo('begin test 227');
for(var i = 536870911; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test228
WScript.Echo('begin test 228');
for(var i = -536870912; i < -536870903; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test229
WScript.Echo('begin test 229');
for(var i = -536870912; i < -536870903; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test46() {
var x;
var y;
var result;
var check;
// Test230
WScript.Echo('begin test 230');
for(var i = -536870912; i > -536870921; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test231
WScript.Echo('begin test 231');
for(var i = -536870912; i > -536870921; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test232
WScript.Echo('begin test 232');
for(var i = -536870912; i <= -536870903; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test233
WScript.Echo('begin test 233');
for(var i = -536870912; i <= -536870903; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test234
WScript.Echo('begin test 234');
for(var i = -536870912; i >= -536870921; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test47() {
var x;
var y;
var result;
var check;
// Test235
WScript.Echo('begin test 235');
for(var i = -536870912; i >= -536870921; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test236
WScript.Echo('begin test 236');
for(var i = -536870912; i != -536870903; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test237
WScript.Echo('begin test 237');
for(var i = -536870912; i != -536870903; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test238
WScript.Echo('begin test 238');
for(var i = -536870912; i != -536870921; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test239
WScript.Echo('begin test 239');
for(var i = -536870912; i != -536870921; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test48() {
var x;
var y;
var result;
var check;
// Test240
WScript.Echo('begin test 240');
for(var i = -536870912; i < -536870903; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test241
WScript.Echo('begin test 241');
for(var i = -536870912; i > -536870921; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test242
WScript.Echo('begin test 242');
for(var i = -536870912; i <= -536870903; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test243
WScript.Echo('begin test 243');
for(var i = -536870912; i >= -536870921; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test244
WScript.Echo('begin test 244');
for(var i = -536870912; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test49() {
var x;
var y;
var result;
var check;
// Test245
WScript.Echo('begin test 245');
for(var i = 1073741822; i < 1073741831; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test246
WScript.Echo('begin test 246');
for(var i = 1073741822; i < 1073741831; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test247
WScript.Echo('begin test 247');
for(var i = 1073741822; i > 1073741813; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test248
WScript.Echo('begin test 248');
for(var i = 1073741822; i > 1073741813; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test249
WScript.Echo('begin test 249');
for(var i = 1073741822; i <= 1073741831; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test50() {
var x;
var y;
var result;
var check;
// Test250
WScript.Echo('begin test 250');
for(var i = 1073741822; i <= 1073741831; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test251
WScript.Echo('begin test 251');
for(var i = 1073741822; i >= 1073741813; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test252
WScript.Echo('begin test 252');
for(var i = 1073741822; i >= 1073741813; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test253
WScript.Echo('begin test 253');
for(var i = 1073741822; i != 1073741831; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test254
WScript.Echo('begin test 254');
for(var i = 1073741822; i != 1073741831; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test51() {
var x;
var y;
var result;
var check;
// Test255
WScript.Echo('begin test 255');
for(var i = 1073741822; i != 1073741813; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test256
WScript.Echo('begin test 256');
for(var i = 1073741822; i != 1073741813; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test257
WScript.Echo('begin test 257');
for(var i = 1073741822; i < 1073741831; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test258
WScript.Echo('begin test 258');
for(var i = 1073741822; i > 1073741813; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test259
WScript.Echo('begin test 259');
for(var i = 1073741822; i <= 1073741831; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test52() {
var x;
var y;
var result;
var check;
// Test260
WScript.Echo('begin test 260');
for(var i = 1073741822; i >= 1073741813; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test261
WScript.Echo('begin test 261');
for(var i = 1073741822; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test262
WScript.Echo('begin test 262');
for(var i = 1073741822; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test263
WScript.Echo('begin test 263');
for(var i = 1073741822; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test264
WScript.Echo('begin test 264');
for(var i = 1073741822; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test53() {
var x;
var y;
var result;
var check;
// Test265
WScript.Echo('begin test 265');
for(var i = 1073741822; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test266
WScript.Echo('begin test 266');
for(var i = 1073741823; i < 1073741832; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test267
WScript.Echo('begin test 267');
for(var i = 1073741823; i < 1073741832; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test268
WScript.Echo('begin test 268');
for(var i = 1073741823; i > 1073741814; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test269
WScript.Echo('begin test 269');
for(var i = 1073741823; i > 1073741814; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test54() {
var x;
var y;
var result;
var check;
// Test270
WScript.Echo('begin test 270');
for(var i = 1073741823; i <= 1073741832; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test271
WScript.Echo('begin test 271');
for(var i = 1073741823; i <= 1073741832; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test272
WScript.Echo('begin test 272');
for(var i = 1073741823; i >= 1073741814; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test273
WScript.Echo('begin test 273');
for(var i = 1073741823; i >= 1073741814; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test274
WScript.Echo('begin test 274');
for(var i = 1073741823; i != 1073741832; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test55() {
var x;
var y;
var result;
var check;
// Test275
WScript.Echo('begin test 275');
for(var i = 1073741823; i != 1073741832; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test276
WScript.Echo('begin test 276');
for(var i = 1073741823; i != 1073741814; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test277
WScript.Echo('begin test 277');
for(var i = 1073741823; i != 1073741814; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test278
WScript.Echo('begin test 278');
for(var i = 1073741823; i < 1073741832; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test279
WScript.Echo('begin test 279');
for(var i = 1073741823; i > 1073741814; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test56() {
var x;
var y;
var result;
var check;
// Test280
WScript.Echo('begin test 280');
for(var i = 1073741823; i <= 1073741832; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test281
WScript.Echo('begin test 281');
for(var i = 1073741823; i >= 1073741814; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test282
WScript.Echo('begin test 282');
for(var i = 1073741823; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test283
WScript.Echo('begin test 283');
for(var i = 1073741823; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test284
WScript.Echo('begin test 284');
for(var i = 1073741823; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test57() {
var x;
var y;
var result;
var check;
// Test285
WScript.Echo('begin test 285');
for(var i = 1073741823; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test286
WScript.Echo('begin test 286');
for(var i = 1073741823; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test287
WScript.Echo('begin test 287');
for(var i = 1073741824; i < 1073741833; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test288
WScript.Echo('begin test 288');
for(var i = 1073741824; i < 1073741833; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test289
WScript.Echo('begin test 289');
for(var i = 1073741824; i > 1073741815; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test58() {
var x;
var y;
var result;
var check;
// Test290
WScript.Echo('begin test 290');
for(var i = 1073741824; i > 1073741815; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test291
WScript.Echo('begin test 291');
for(var i = 1073741824; i <= 1073741833; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test292
WScript.Echo('begin test 292');
for(var i = 1073741824; i <= 1073741833; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test293
WScript.Echo('begin test 293');
for(var i = 1073741824; i >= 1073741815; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test294
WScript.Echo('begin test 294');
for(var i = 1073741824; i >= 1073741815; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test59() {
var x;
var y;
var result;
var check;
// Test295
WScript.Echo('begin test 295');
for(var i = 1073741824; i != 1073741833; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test296
WScript.Echo('begin test 296');
for(var i = 1073741824; i != 1073741833; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test297
WScript.Echo('begin test 297');
for(var i = 1073741824; i != 1073741815; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test298
WScript.Echo('begin test 298');
for(var i = 1073741824; i != 1073741815; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test299
WScript.Echo('begin test 299');
for(var i = 1073741824; i < 1073741833; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test60() {
var x;
var y;
var result;
var check;
// Test300
WScript.Echo('begin test 300');
for(var i = 1073741824; i > 1073741815; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test301
WScript.Echo('begin test 301');
for(var i = 1073741824; i <= 1073741833; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test302
WScript.Echo('begin test 302');
for(var i = 1073741824; i >= 1073741815; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test303
WScript.Echo('begin test 303');
for(var i = 1073741824; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test304
WScript.Echo('begin test 304');
for(var i = 1073741824; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test61() {
var x;
var y;
var result;
var check;
// Test305
WScript.Echo('begin test 305');
for(var i = 1073741824; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test306
WScript.Echo('begin test 306');
for(var i = 1073741824; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test307
WScript.Echo('begin test 307');
for(var i = 1073741824; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test308
WScript.Echo('begin test 308');
for(var i = 1073741825; i < 1073741834; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test309
WScript.Echo('begin test 309');
for(var i = 1073741825; i < 1073741834; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test62() {
var x;
var y;
var result;
var check;
// Test310
WScript.Echo('begin test 310');
for(var i = 1073741825; i > 1073741816; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test311
WScript.Echo('begin test 311');
for(var i = 1073741825; i > 1073741816; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test312
WScript.Echo('begin test 312');
for(var i = 1073741825; i <= 1073741834; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test313
WScript.Echo('begin test 313');
for(var i = 1073741825; i <= 1073741834; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test314
WScript.Echo('begin test 314');
for(var i = 1073741825; i >= 1073741816; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test63() {
var x;
var y;
var result;
var check;
// Test315
WScript.Echo('begin test 315');
for(var i = 1073741825; i >= 1073741816; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test316
WScript.Echo('begin test 316');
for(var i = 1073741825; i != 1073741834; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test317
WScript.Echo('begin test 317');
for(var i = 1073741825; i != 1073741834; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test318
WScript.Echo('begin test 318');
for(var i = 1073741825; i != 1073741816; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test319
WScript.Echo('begin test 319');
for(var i = 1073741825; i != 1073741816; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test64() {
var x;
var y;
var result;
var check;
// Test320
WScript.Echo('begin test 320');
for(var i = 1073741825; i < 1073741834; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test321
WScript.Echo('begin test 321');
for(var i = 1073741825; i > 1073741816; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test322
WScript.Echo('begin test 322');
for(var i = 1073741825; i <= 1073741834; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test323
WScript.Echo('begin test 323');
for(var i = 1073741825; i >= 1073741816; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test324
WScript.Echo('begin test 324');
for(var i = 1073741825; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test65() {
var x;
var y;
var result;
var check;
// Test325
WScript.Echo('begin test 325');
for(var i = 1073741825; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test326
WScript.Echo('begin test 326');
for(var i = 1073741825; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test327
WScript.Echo('begin test 327');
for(var i = 1073741825; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test328
WScript.Echo('begin test 328');
for(var i = 1073741825; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test329
WScript.Echo('begin test 329');
for(var i = -1073741823; i < -1073741814; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test66() {
var x;
var y;
var result;
var check;
// Test330
WScript.Echo('begin test 330');
for(var i = -1073741823; i < -1073741814; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test331
WScript.Echo('begin test 331');
for(var i = -1073741823; i > -1073741832; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test332
WScript.Echo('begin test 332');
for(var i = -1073741823; i > -1073741832; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test333
WScript.Echo('begin test 333');
for(var i = -1073741823; i <= -1073741814; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test334
WScript.Echo('begin test 334');
for(var i = -1073741823; i <= -1073741814; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test67() {
var x;
var y;
var result;
var check;
// Test335
WScript.Echo('begin test 335');
for(var i = -1073741823; i >= -1073741832; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test336
WScript.Echo('begin test 336');
for(var i = -1073741823; i >= -1073741832; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test337
WScript.Echo('begin test 337');
for(var i = -1073741823; i != -1073741814; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test338
WScript.Echo('begin test 338');
for(var i = -1073741823; i != -1073741814; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test339
WScript.Echo('begin test 339');
for(var i = -1073741823; i != -1073741832; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test68() {
var x;
var y;
var result;
var check;
// Test340
WScript.Echo('begin test 340');
for(var i = -1073741823; i != -1073741832; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test341
WScript.Echo('begin test 341');
for(var i = -1073741823; i < -1073741814; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test342
WScript.Echo('begin test 342');
for(var i = -1073741823; i > -1073741832; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test343
WScript.Echo('begin test 343');
for(var i = -1073741823; i <= -1073741814; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test344
WScript.Echo('begin test 344');
for(var i = -1073741823; i >= -1073741832; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test69() {
var x;
var y;
var result;
var check;
// Test345
WScript.Echo('begin test 345');
for(var i = -1073741823; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test346
WScript.Echo('begin test 346');
for(var i = -1073741824; i < -1073741815; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test347
WScript.Echo('begin test 347');
for(var i = -1073741824; i < -1073741815; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test348
WScript.Echo('begin test 348');
for(var i = -1073741824; i > -1073741833; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test349
WScript.Echo('begin test 349');
for(var i = -1073741824; i > -1073741833; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test70() {
var x;
var y;
var result;
var check;
// Test350
WScript.Echo('begin test 350');
for(var i = -1073741824; i <= -1073741815; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test351
WScript.Echo('begin test 351');
for(var i = -1073741824; i <= -1073741815; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test352
WScript.Echo('begin test 352');
for(var i = -1073741824; i >= -1073741833; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test353
WScript.Echo('begin test 353');
for(var i = -1073741824; i >= -1073741833; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test354
WScript.Echo('begin test 354');
for(var i = -1073741824; i != -1073741815; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test71() {
var x;
var y;
var result;
var check;
// Test355
WScript.Echo('begin test 355');
for(var i = -1073741824; i != -1073741815; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test356
WScript.Echo('begin test 356');
for(var i = -1073741824; i != -1073741833; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test357
WScript.Echo('begin test 357');
for(var i = -1073741824; i != -1073741833; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test358
WScript.Echo('begin test 358');
for(var i = -1073741824; i < -1073741815; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test359
WScript.Echo('begin test 359');
for(var i = -1073741824; i > -1073741833; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test72() {
var x;
var y;
var result;
var check;
// Test360
WScript.Echo('begin test 360');
for(var i = -1073741824; i <= -1073741815; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test361
WScript.Echo('begin test 361');
for(var i = -1073741824; i >= -1073741833; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test362
WScript.Echo('begin test 362');
for(var i = -1073741824; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test363
WScript.Echo('begin test 363');
for(var i = -1073741825; i < -1073741816; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test364
WScript.Echo('begin test 364');
for(var i = -1073741825; i < -1073741816; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test73() {
var x;
var y;
var result;
var check;
// Test365
WScript.Echo('begin test 365');
for(var i = -1073741825; i > -1073741834; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test366
WScript.Echo('begin test 366');
for(var i = -1073741825; i > -1073741834; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test367
WScript.Echo('begin test 367');
for(var i = -1073741825; i <= -1073741816; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test368
WScript.Echo('begin test 368');
for(var i = -1073741825; i <= -1073741816; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test369
WScript.Echo('begin test 369');
for(var i = -1073741825; i >= -1073741834; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test74() {
var x;
var y;
var result;
var check;
// Test370
WScript.Echo('begin test 370');
for(var i = -1073741825; i >= -1073741834; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test371
WScript.Echo('begin test 371');
for(var i = -1073741825; i != -1073741816; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test372
WScript.Echo('begin test 372');
for(var i = -1073741825; i != -1073741816; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test373
WScript.Echo('begin test 373');
for(var i = -1073741825; i != -1073741834; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test374
WScript.Echo('begin test 374');
for(var i = -1073741825; i != -1073741834; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test75() {
var x;
var y;
var result;
var check;
// Test375
WScript.Echo('begin test 375');
for(var i = -1073741825; i < -1073741816; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test376
WScript.Echo('begin test 376');
for(var i = -1073741825; i > -1073741834; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test377
WScript.Echo('begin test 377');
for(var i = -1073741825; i <= -1073741816; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test378
WScript.Echo('begin test 378');
for(var i = -1073741825; i >= -1073741834; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test379
WScript.Echo('begin test 379');
for(var i = -1073741825; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test76() {
var x;
var y;
var result;
var check;
// Test380
WScript.Echo('begin test 380');
for(var i = -1073741826; i < -1073741817; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test381
WScript.Echo('begin test 381');
for(var i = -1073741826; i < -1073741817; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test382
WScript.Echo('begin test 382');
for(var i = -1073741826; i > -1073741835; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test383
WScript.Echo('begin test 383');
for(var i = -1073741826; i > -1073741835; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test384
WScript.Echo('begin test 384');
for(var i = -1073741826; i <= -1073741817; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test77() {
var x;
var y;
var result;
var check;
// Test385
WScript.Echo('begin test 385');
for(var i = -1073741826; i <= -1073741817; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test386
WScript.Echo('begin test 386');
for(var i = -1073741826; i >= -1073741835; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test387
WScript.Echo('begin test 387');
for(var i = -1073741826; i >= -1073741835; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test388
WScript.Echo('begin test 388');
for(var i = -1073741826; i != -1073741817; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test389
WScript.Echo('begin test 389');
for(var i = -1073741826; i != -1073741817; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test78() {
var x;
var y;
var result;
var check;
// Test390
WScript.Echo('begin test 390');
for(var i = -1073741826; i != -1073741835; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test391
WScript.Echo('begin test 391');
for(var i = -1073741826; i != -1073741835; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test392
WScript.Echo('begin test 392');
for(var i = -1073741826; i < -1073741817; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test393
WScript.Echo('begin test 393');
for(var i = -1073741826; i > -1073741835; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test394
WScript.Echo('begin test 394');
for(var i = -1073741826; i <= -1073741817; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test79() {
var x;
var y;
var result;
var check;
// Test395
WScript.Echo('begin test 395');
for(var i = -1073741826; i >= -1073741835; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test396
WScript.Echo('begin test 396');
for(var i = -1073741826; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test397
WScript.Echo('begin test 397');
for(var i = 2147483646; i < 2147483655; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test398
WScript.Echo('begin test 398');
for(var i = 2147483646; i < 2147483655; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test399
WScript.Echo('begin test 399');
for(var i = 2147483646; i > 2147483637; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test80() {
var x;
var y;
var result;
var check;
// Test400
WScript.Echo('begin test 400');
for(var i = 2147483646; i > 2147483637; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test401
WScript.Echo('begin test 401');
for(var i = 2147483646; i <= 2147483655; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test402
WScript.Echo('begin test 402');
for(var i = 2147483646; i <= 2147483655; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test403
WScript.Echo('begin test 403');
for(var i = 2147483646; i >= 2147483637; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test404
WScript.Echo('begin test 404');
for(var i = 2147483646; i >= 2147483637; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test81() {
var x;
var y;
var result;
var check;
// Test405
WScript.Echo('begin test 405');
for(var i = 2147483646; i != 2147483655; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test406
WScript.Echo('begin test 406');
for(var i = 2147483646; i != 2147483655; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test407
WScript.Echo('begin test 407');
for(var i = 2147483646; i != 2147483637; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test408
WScript.Echo('begin test 408');
for(var i = 2147483646; i != 2147483637; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test409
WScript.Echo('begin test 409');
for(var i = 2147483646; i < 2147483655; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test82() {
var x;
var y;
var result;
var check;
// Test410
WScript.Echo('begin test 410');
for(var i = 2147483646; i > 2147483637; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test411
WScript.Echo('begin test 411');
for(var i = 2147483646; i <= 2147483655; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test412
WScript.Echo('begin test 412');
for(var i = 2147483646; i >= 2147483637; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test413
WScript.Echo('begin test 413');
for(var i = 2147483646; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test414
WScript.Echo('begin test 414');
for(var i = 2147483646; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test83() {
var x;
var y;
var result;
var check;
// Test415
WScript.Echo('begin test 415');
for(var i = 2147483646; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test416
WScript.Echo('begin test 416');
for(var i = 2147483646; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test417
WScript.Echo('begin test 417');
for(var i = 2147483646; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test418
WScript.Echo('begin test 418');
for(var i = 2147483647; i < 2147483656; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test419
WScript.Echo('begin test 419');
for(var i = 2147483647; i < 2147483656; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test84() {
var x;
var y;
var result;
var check;
// Test420
WScript.Echo('begin test 420');
for(var i = 2147483647; i > 2147483638; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test421
WScript.Echo('begin test 421');
for(var i = 2147483647; i > 2147483638; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test422
WScript.Echo('begin test 422');
for(var i = 2147483647; i <= 2147483656; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test423
WScript.Echo('begin test 423');
for(var i = 2147483647; i <= 2147483656; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test424
WScript.Echo('begin test 424');
for(var i = 2147483647; i >= 2147483638; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test85() {
var x;
var y;
var result;
var check;
// Test425
WScript.Echo('begin test 425');
for(var i = 2147483647; i >= 2147483638; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test426
WScript.Echo('begin test 426');
for(var i = 2147483647; i != 2147483656; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test427
WScript.Echo('begin test 427');
for(var i = 2147483647; i != 2147483656; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test428
WScript.Echo('begin test 428');
for(var i = 2147483647; i != 2147483638; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test429
WScript.Echo('begin test 429');
for(var i = 2147483647; i != 2147483638; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test86() {
var x;
var y;
var result;
var check;
// Test430
WScript.Echo('begin test 430');
for(var i = 2147483647; i < 2147483656; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test431
WScript.Echo('begin test 431');
for(var i = 2147483647; i > 2147483638; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test432
WScript.Echo('begin test 432');
for(var i = 2147483647; i <= 2147483656; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test433
WScript.Echo('begin test 433');
for(var i = 2147483647; i >= 2147483638; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test434
WScript.Echo('begin test 434');
for(var i = 2147483647; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test87() {
var x;
var y;
var result;
var check;
// Test435
WScript.Echo('begin test 435');
for(var i = 2147483647; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test436
WScript.Echo('begin test 436');
for(var i = 2147483647; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test437
WScript.Echo('begin test 437');
for(var i = 2147483647; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test438
WScript.Echo('begin test 438');
for(var i = 2147483647; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test439
WScript.Echo('begin test 439');
for(var i = 2147483648; i < 2147483657; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test88() {
var x;
var y;
var result;
var check;
// Test440
WScript.Echo('begin test 440');
for(var i = 2147483648; i < 2147483657; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test441
WScript.Echo('begin test 441');
for(var i = 2147483648; i > 2147483639; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test442
WScript.Echo('begin test 442');
for(var i = 2147483648; i > 2147483639; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test443
WScript.Echo('begin test 443');
for(var i = 2147483648; i <= 2147483657; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test444
WScript.Echo('begin test 444');
for(var i = 2147483648; i <= 2147483657; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test89() {
var x;
var y;
var result;
var check;
// Test445
WScript.Echo('begin test 445');
for(var i = 2147483648; i >= 2147483639; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test446
WScript.Echo('begin test 446');
for(var i = 2147483648; i >= 2147483639; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test447
WScript.Echo('begin test 447');
for(var i = 2147483648; i != 2147483657; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test448
WScript.Echo('begin test 448');
for(var i = 2147483648; i != 2147483657; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test449
WScript.Echo('begin test 449');
for(var i = 2147483648; i != 2147483639; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test90() {
var x;
var y;
var result;
var check;
// Test450
WScript.Echo('begin test 450');
for(var i = 2147483648; i != 2147483639; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test451
WScript.Echo('begin test 451');
for(var i = 2147483648; i < 2147483657; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test452
WScript.Echo('begin test 452');
for(var i = 2147483648; i > 2147483639; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test453
WScript.Echo('begin test 453');
for(var i = 2147483648; i <= 2147483657; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test454
WScript.Echo('begin test 454');
for(var i = 2147483648; i >= 2147483639; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test91() {
var x;
var y;
var result;
var check;
// Test455
WScript.Echo('begin test 455');
for(var i = 2147483648; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test456
WScript.Echo('begin test 456');
for(var i = 2147483648; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test457
WScript.Echo('begin test 457');
for(var i = 2147483648; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test458
WScript.Echo('begin test 458');
for(var i = 2147483648; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test459
WScript.Echo('begin test 459');
for(var i = 2147483648; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test92() {
var x;
var y;
var result;
var check;
// Test460
WScript.Echo('begin test 460');
for(var i = 2147483649; i < 2147483658; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test461
WScript.Echo('begin test 461');
for(var i = 2147483649; i < 2147483658; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test462
WScript.Echo('begin test 462');
for(var i = 2147483649; i > 2147483640; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test463
WScript.Echo('begin test 463');
for(var i = 2147483649; i > 2147483640; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test464
WScript.Echo('begin test 464');
for(var i = 2147483649; i <= 2147483658; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test93() {
var x;
var y;
var result;
var check;
// Test465
WScript.Echo('begin test 465');
for(var i = 2147483649; i <= 2147483658; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test466
WScript.Echo('begin test 466');
for(var i = 2147483649; i >= 2147483640; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test467
WScript.Echo('begin test 467');
for(var i = 2147483649; i >= 2147483640; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test468
WScript.Echo('begin test 468');
for(var i = 2147483649; i != 2147483658; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test469
WScript.Echo('begin test 469');
for(var i = 2147483649; i != 2147483658; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test94() {
var x;
var y;
var result;
var check;
// Test470
WScript.Echo('begin test 470');
for(var i = 2147483649; i != 2147483640; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test471
WScript.Echo('begin test 471');
for(var i = 2147483649; i != 2147483640; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test472
WScript.Echo('begin test 472');
for(var i = 2147483649; i < 2147483658; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test473
WScript.Echo('begin test 473');
for(var i = 2147483649; i > 2147483640; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test474
WScript.Echo('begin test 474');
for(var i = 2147483649; i <= 2147483658; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test95() {
var x;
var y;
var result;
var check;
// Test475
WScript.Echo('begin test 475');
for(var i = 2147483649; i >= 2147483640; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test476
WScript.Echo('begin test 476');
for(var i = 2147483649; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test477
WScript.Echo('begin test 477');
for(var i = 2147483649; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test478
WScript.Echo('begin test 478');
for(var i = 2147483649; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test479
WScript.Echo('begin test 479');
for(var i = 2147483649; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test96() {
var x;
var y;
var result;
var check;
// Test480
WScript.Echo('begin test 480');
for(var i = 2147483649; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test481
WScript.Echo('begin test 481');
for(var i = -2147483647; i < -2147483638; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test482
WScript.Echo('begin test 482');
for(var i = -2147483647; i < -2147483638; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test483
WScript.Echo('begin test 483');
for(var i = -2147483647; i > -2147483656; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test484
WScript.Echo('begin test 484');
for(var i = -2147483647; i > -2147483656; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test97() {
var x;
var y;
var result;
var check;
// Test485
WScript.Echo('begin test 485');
for(var i = -2147483647; i <= -2147483638; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test486
WScript.Echo('begin test 486');
for(var i = -2147483647; i <= -2147483638; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test487
WScript.Echo('begin test 487');
for(var i = -2147483647; i >= -2147483656; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test488
WScript.Echo('begin test 488');
for(var i = -2147483647; i >= -2147483656; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test489
WScript.Echo('begin test 489');
for(var i = -2147483647; i != -2147483638; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test98() {
var x;
var y;
var result;
var check;
// Test490
WScript.Echo('begin test 490');
for(var i = -2147483647; i != -2147483638; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test491
WScript.Echo('begin test 491');
for(var i = -2147483647; i != -2147483656; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test492
WScript.Echo('begin test 492');
for(var i = -2147483647; i != -2147483656; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test493
WScript.Echo('begin test 493');
for(var i = -2147483647; i < -2147483638; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test494
WScript.Echo('begin test 494');
for(var i = -2147483647; i > -2147483656; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test99() {
var x;
var y;
var result;
var check;
// Test495
WScript.Echo('begin test 495');
for(var i = -2147483647; i <= -2147483638; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test496
WScript.Echo('begin test 496');
for(var i = -2147483647; i >= -2147483656; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test497
WScript.Echo('begin test 497');
for(var i = -2147483647; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test498
WScript.Echo('begin test 498');
for(var i = -2147483648; i < -2147483639; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test499
WScript.Echo('begin test 499');
for(var i = -2147483648; i < -2147483639; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test100() {
var x;
var y;
var result;
var check;
// Test500
WScript.Echo('begin test 500');
for(var i = -2147483648; i > -2147483657; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test501
WScript.Echo('begin test 501');
for(var i = -2147483648; i > -2147483657; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test502
WScript.Echo('begin test 502');
for(var i = -2147483648; i <= -2147483639; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test503
WScript.Echo('begin test 503');
for(var i = -2147483648; i <= -2147483639; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test504
WScript.Echo('begin test 504');
for(var i = -2147483648; i >= -2147483657; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test101() {
var x;
var y;
var result;
var check;
// Test505
WScript.Echo('begin test 505');
for(var i = -2147483648; i >= -2147483657; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test506
WScript.Echo('begin test 506');
for(var i = -2147483648; i != -2147483639; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test507
WScript.Echo('begin test 507');
for(var i = -2147483648; i != -2147483639; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test508
WScript.Echo('begin test 508');
for(var i = -2147483648; i != -2147483657; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test509
WScript.Echo('begin test 509');
for(var i = -2147483648; i != -2147483657; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test102() {
var x;
var y;
var result;
var check;
// Test510
WScript.Echo('begin test 510');
for(var i = -2147483648; i < -2147483639; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test511
WScript.Echo('begin test 511');
for(var i = -2147483648; i > -2147483657; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test512
WScript.Echo('begin test 512');
for(var i = -2147483648; i <= -2147483639; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test513
WScript.Echo('begin test 513');
for(var i = -2147483648; i >= -2147483657; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test514
WScript.Echo('begin test 514');
for(var i = -2147483648; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test103() {
var x;
var y;
var result;
var check;
// Test515
WScript.Echo('begin test 515');
for(var i = -2147483649; i < -2147483640; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test516
WScript.Echo('begin test 516');
for(var i = -2147483649; i < -2147483640; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test517
WScript.Echo('begin test 517');
for(var i = -2147483649; i > -2147483658; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test518
WScript.Echo('begin test 518');
for(var i = -2147483649; i > -2147483658; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test519
WScript.Echo('begin test 519');
for(var i = -2147483649; i <= -2147483640; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test104() {
var x;
var y;
var result;
var check;
// Test520
WScript.Echo('begin test 520');
for(var i = -2147483649; i <= -2147483640; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test521
WScript.Echo('begin test 521');
for(var i = -2147483649; i >= -2147483658; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test522
WScript.Echo('begin test 522');
for(var i = -2147483649; i >= -2147483658; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test523
WScript.Echo('begin test 523');
for(var i = -2147483649; i != -2147483640; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test524
WScript.Echo('begin test 524');
for(var i = -2147483649; i != -2147483640; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test105() {
var x;
var y;
var result;
var check;
// Test525
WScript.Echo('begin test 525');
for(var i = -2147483649; i != -2147483658; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test526
WScript.Echo('begin test 526');
for(var i = -2147483649; i != -2147483658; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test527
WScript.Echo('begin test 527');
for(var i = -2147483649; i < -2147483640; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test528
WScript.Echo('begin test 528');
for(var i = -2147483649; i > -2147483658; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test529
WScript.Echo('begin test 529');
for(var i = -2147483649; i <= -2147483640; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test106() {
var x;
var y;
var result;
var check;
// Test530
WScript.Echo('begin test 530');
for(var i = -2147483649; i >= -2147483658; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test531
WScript.Echo('begin test 531');
for(var i = -2147483649; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test532
WScript.Echo('begin test 532');
for(var i = -2147483650; i < -2147483641; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test533
WScript.Echo('begin test 533');
for(var i = -2147483650; i < -2147483641; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test534
WScript.Echo('begin test 534');
for(var i = -2147483650; i > -2147483659; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test107() {
var x;
var y;
var result;
var check;
// Test535
WScript.Echo('begin test 535');
for(var i = -2147483650; i > -2147483659; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test536
WScript.Echo('begin test 536');
for(var i = -2147483650; i <= -2147483641; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test537
WScript.Echo('begin test 537');
for(var i = -2147483650; i <= -2147483641; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test538
WScript.Echo('begin test 538');
for(var i = -2147483650; i >= -2147483659; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test539
WScript.Echo('begin test 539');
for(var i = -2147483650; i >= -2147483659; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test108() {
var x;
var y;
var result;
var check;
// Test540
WScript.Echo('begin test 540');
for(var i = -2147483650; i != -2147483641; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test541
WScript.Echo('begin test 541');
for(var i = -2147483650; i != -2147483641; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test542
WScript.Echo('begin test 542');
for(var i = -2147483650; i != -2147483659; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test543
WScript.Echo('begin test 543');
for(var i = -2147483650; i != -2147483659; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test544
WScript.Echo('begin test 544');
for(var i = -2147483650; i < -2147483641; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test109() {
var x;
var y;
var result;
var check;
// Test545
WScript.Echo('begin test 545');
for(var i = -2147483650; i > -2147483659; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test546
WScript.Echo('begin test 546');
for(var i = -2147483650; i <= -2147483641; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test547
WScript.Echo('begin test 547');
for(var i = -2147483650; i >= -2147483659; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test548
WScript.Echo('begin test 548');
for(var i = -2147483650; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test549
WScript.Echo('begin test 549');
for(var i = 4294967295; i < 4294967304; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test110() {
var x;
var y;
var result;
var check;
// Test550
WScript.Echo('begin test 550');
for(var i = 4294967295; i < 4294967304; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test551
WScript.Echo('begin test 551');
for(var i = 4294967295; i > 4294967286; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test552
WScript.Echo('begin test 552');
for(var i = 4294967295; i > 4294967286; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test553
WScript.Echo('begin test 553');
for(var i = 4294967295; i <= 4294967304; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test554
WScript.Echo('begin test 554');
for(var i = 4294967295; i <= 4294967304; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test111() {
var x;
var y;
var result;
var check;
// Test555
WScript.Echo('begin test 555');
for(var i = 4294967295; i >= 4294967286; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test556
WScript.Echo('begin test 556');
for(var i = 4294967295; i >= 4294967286; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test557
WScript.Echo('begin test 557');
for(var i = 4294967295; i != 4294967304; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test558
WScript.Echo('begin test 558');
for(var i = 4294967295; i != 4294967304; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test559
WScript.Echo('begin test 559');
for(var i = 4294967295; i != 4294967286; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test112() {
var x;
var y;
var result;
var check;
// Test560
WScript.Echo('begin test 560');
for(var i = 4294967295; i != 4294967286; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test561
WScript.Echo('begin test 561');
for(var i = 4294967295; i < 4294967304; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test562
WScript.Echo('begin test 562');
for(var i = 4294967295; i > 4294967286; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test563
WScript.Echo('begin test 563');
for(var i = 4294967295; i <= 4294967304; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test564
WScript.Echo('begin test 564');
for(var i = 4294967295; i >= 4294967286; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test113() {
var x;
var y;
var result;
var check;
// Test565
WScript.Echo('begin test 565');
for(var i = 4294967295; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test566
WScript.Echo('begin test 566');
for(var i = 4294967295; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test567
WScript.Echo('begin test 567');
for(var i = 4294967295; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test568
WScript.Echo('begin test 568');
for(var i = 4294967295; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test569
WScript.Echo('begin test 569');
for(var i = 4294967295; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test114() {
var x;
var y;
var result;
var check;
// Test570
WScript.Echo('begin test 570');
for(var i = 4294967296; i < 4294967305; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test571
WScript.Echo('begin test 571');
for(var i = 4294967296; i < 4294967305; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test572
WScript.Echo('begin test 572');
for(var i = 4294967296; i > 4294967287; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test573
WScript.Echo('begin test 573');
for(var i = 4294967296; i > 4294967287; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test574
WScript.Echo('begin test 574');
for(var i = 4294967296; i <= 4294967305; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test115() {
var x;
var y;
var result;
var check;
// Test575
WScript.Echo('begin test 575');
for(var i = 4294967296; i <= 4294967305; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test576
WScript.Echo('begin test 576');
for(var i = 4294967296; i >= 4294967287; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test577
WScript.Echo('begin test 577');
for(var i = 4294967296; i >= 4294967287; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test578
WScript.Echo('begin test 578');
for(var i = 4294967296; i != 4294967305; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test579
WScript.Echo('begin test 579');
for(var i = 4294967296; i != 4294967305; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test116() {
var x;
var y;
var result;
var check;
// Test580
WScript.Echo('begin test 580');
for(var i = 4294967296; i != 4294967287; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test581
WScript.Echo('begin test 581');
for(var i = 4294967296; i != 4294967287; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test582
WScript.Echo('begin test 582');
for(var i = 4294967296; i < 4294967305; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test583
WScript.Echo('begin test 583');
for(var i = 4294967296; i > 4294967287; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test584
WScript.Echo('begin test 584');
for(var i = 4294967296; i <= 4294967305; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test117() {
var x;
var y;
var result;
var check;
// Test585
WScript.Echo('begin test 585');
for(var i = 4294967296; i >= 4294967287; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test586
WScript.Echo('begin test 586');
for(var i = 4294967296; i > 0; i >>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test587
WScript.Echo('begin test 587');
for(var i = 4294967296; i > 0; i >>>= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test588
WScript.Echo('begin test 588');
for(var i = 4294967296; i > 0; i >>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test589
WScript.Echo('begin test 589');
for(var i = 4294967296; i > 0; i >>>= 2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test118() {
var x;
var y;
var result;
var check;
// Test590
WScript.Echo('begin test 590');
for(var i = 4294967296; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test591
WScript.Echo('begin test 591');
for(var i = -4294967295; i < -4294967286; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test592
WScript.Echo('begin test 592');
for(var i = -4294967295; i < -4294967286; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test593
WScript.Echo('begin test 593');
for(var i = -4294967295; i > -4294967304; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test594
WScript.Echo('begin test 594');
for(var i = -4294967295; i > -4294967304; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test119() {
var x;
var y;
var result;
var check;
// Test595
WScript.Echo('begin test 595');
for(var i = -4294967295; i <= -4294967286; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test596
WScript.Echo('begin test 596');
for(var i = -4294967295; i <= -4294967286; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test597
WScript.Echo('begin test 597');
for(var i = -4294967295; i >= -4294967304; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test598
WScript.Echo('begin test 598');
for(var i = -4294967295; i >= -4294967304; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test599
WScript.Echo('begin test 599');
for(var i = -4294967295; i != -4294967286; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test120() {
var x;
var y;
var result;
var check;
// Test600
WScript.Echo('begin test 600');
for(var i = -4294967295; i != -4294967286; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test601
WScript.Echo('begin test 601');
for(var i = -4294967295; i != -4294967304; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test602
WScript.Echo('begin test 602');
for(var i = -4294967295; i != -4294967304; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test603
WScript.Echo('begin test 603');
for(var i = -4294967295; i < -4294967286; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test604
WScript.Echo('begin test 604');
for(var i = -4294967295; i > -4294967304; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test121() {
var x;
var y;
var result;
var check;
// Test605
WScript.Echo('begin test 605');
for(var i = -4294967295; i <= -4294967286; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test606
WScript.Echo('begin test 606');
for(var i = -4294967295; i >= -4294967304; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test607
WScript.Echo('begin test 607');
for(var i = -4294967295; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test608
WScript.Echo('begin test 608');
for(var i = -4294967296; i < -4294967287; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test609
WScript.Echo('begin test 609');
for(var i = -4294967296; i < -4294967287; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test122() {
var x;
var y;
var result;
var check;
// Test610
WScript.Echo('begin test 610');
for(var i = -4294967296; i > -4294967305; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test611
WScript.Echo('begin test 611');
for(var i = -4294967296; i > -4294967305; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test612
WScript.Echo('begin test 612');
for(var i = -4294967296; i <= -4294967287; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test613
WScript.Echo('begin test 613');
for(var i = -4294967296; i <= -4294967287; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test614
WScript.Echo('begin test 614');
for(var i = -4294967296; i >= -4294967305; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test123() {
var x;
var y;
var result;
var check;
// Test615
WScript.Echo('begin test 615');
for(var i = -4294967296; i >= -4294967305; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test616
WScript.Echo('begin test 616');
for(var i = -4294967296; i != -4294967287; ++i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test617
WScript.Echo('begin test 617');
for(var i = -4294967296; i != -4294967287; i++) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test618
WScript.Echo('begin test 618');
for(var i = -4294967296; i != -4294967305; i--) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test619
WScript.Echo('begin test 619');
for(var i = -4294967296; i != -4294967305; --i) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
}
function test124() {
var x;
var y;
var result;
var check;
// Test620
WScript.Echo('begin test 620');
for(var i = -4294967296; i < -4294967287; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test621
WScript.Echo('begin test 621');
for(var i = -4294967296; i > -4294967305; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test622
WScript.Echo('begin test 622');
for(var i = -4294967296; i <= -4294967287; i+=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test623
WScript.Echo('begin test 623');
for(var i = -4294967296; i >= -4294967305; i-=2) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
// Test624
WScript.Echo('begin test 624');
for(var i = -4294967296; i != 0; i <<= 1) {
    var z = i < 0 ? -i : i;
    WScript.Echo(Math.abs(z) >= 1000000 ? z % 1000000 : z);
}
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
test40();
test41();
test42();
test43();
test44();
test45();
test46();
test47();
test48();
test49();
test50();
test51();
test52();
test53();
test54();
test55();
test56();
test57();
test58();
test59();
test60();
test61();
test62();
test63();
test64();
test65();
test66();
test67();
test68();
test69();
test70();
test71();
test72();
test73();
test74();
test75();
test76();
test77();
test78();
test79();
test80();
test81();
test82();
test83();
test84();
test85();
test86();
test87();
test88();
test89();
test90();
test91();
test92();
test93();
test94();
test95();
test96();
test97();
test98();
test99();
test100();
test101();
test102();
test103();
test104();
test105();
test106();
test107();
test108();
test109();
test110();
test111();
test112();
test113();
test114();
test115();
test116();
test117();
test118();
test119();
test120();
test121();
test122();
test123();
test124();
WScript.Echo("done");
