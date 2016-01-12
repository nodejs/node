//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test1() { WScript.Echo(/A/.exec({})); };
function test2() { WScript.Echo("".match({})); };
function test2() { WScript.Echo("".indexOf({})); };
function test3() { WScript.Echo("".match()); }; 
function test4() { WScript.Echo("a".match(/a/g)); }; 
function test5() { WScript.Echo("A".replace("C", {})); }; 
function test6() { WScript.Echo("A".replace("C")); }; 
function test7() { WScript.Echo("A".replace()); }; 
function test8() { "A".replace({}, "C"); }; 
function test9() { WScript.Echo("ABCDEF".replace({}, "DEF")); }; 
function test10() { WScript.Echo('fafafa'.replace({}, "X")); }; 
function test11() { var a = {}; WScript.Echo("Aundefined".replace(a.x, Array.isArray)); };
function test12() { WScript.Echo("Aundefined".replace("undefined", function (a) { return a == "Aundefined"; })); };
function test13() { var o = {}; WScript.Echo(/A/.exec(o.x)); };
function test14() { var strvar7 = 'a'; WScript.Echo(strvar7.replace(1, 1).replace(1, 1)); };
function test15() { var o = {}; WScript.Echo("".match(o.x)); }; 
function test16() { try { WScript.Echo(String.prototype.match.call(null, o)); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test17() { try { WScript.Echo(String.prototype.match.apply(Array.a, "a")); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test18() { try { WScript.Echo(String.prototype.match.call("a")); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test19() { try { var o = {}; WScript.Echo(String.prototype.replace.call(null, o)); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test20() { try { WScript.Echo(String.prototype.replace.call(Array.a, "a")); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test21() { WScript.Echo(String.prototype.replace.call("a", String.foo)); }; 
function test22() { try { var o = {}; WScript.Echo(RegExp.prototype.exec.call(null, o.x)); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test23() { try { WScript.Echo(RegExp.prototype.exec.apply(Array.a, "a")); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 
function test24() { try {WScript.Echo(RegExp.prototype.exec.call("a")); } catch(ex) {WScript.Echo('expected : ' + ex.message);}}; 

test1(); test1();
test2(); test2();
test3(); test3();
test4(); test4();
test5(); test5();
test6(); test6();
test7(); test7();
test8(); test8();
test9(); test9();
test10(); test10();
test11(); test11();
test12(); test12();
test13(); test13();
test14(); test14();
test15(); test15();
test16(); test16();
test17(); test17();
test18(); test18();
test19(); test19();
test20(); test20();
test21(); test21();
test22(); test22();
test23(); test23();
test24(); test24();

String.prototype.match = function(x) { WScript.Echo(x instanceof RegExp); return []; }
test2();
String.prototype.replace = function(x) { WScript.Echo(x instanceof Object); return []; }
test9();
