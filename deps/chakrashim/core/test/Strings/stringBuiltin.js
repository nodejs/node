//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


WScript.Echo("Test : var ss = new String(\"String123456EndString\");");
var ss = new String("String123456EndString");

st = ss.substring(3, 4);
WScript.Echo("ss.substring(3,4):  " + st);

st = ss.substring();
WScript.Echo("ss.substring():  " + st);

st = ss.substring(6, 2);
WScript.Echo("ss.substring(6,2):  " + st);

st = ss.substring(-6, 2);
WScript.Echo("ss.substring(-6,2):  " + st);

st = ss.substr(-6, 2);
WScript.Echo("ss.substr(-6,2):  " + st);

st = ss.substr(-1, 4);
WScript.Echo("ss.substr(-1,4):  " + st);

st = ss.slice(2, 7);
WScript.Echo("ss.slice(2, 7):  " + st);

st = ss.slice();
WScript.Echo("ss.slice():  " + st);


WScript.Echo("Test : var ss = new String(\"a\");");
ss = new String("a");

st = ss.substring(3, 4);
WScript.Echo("ss.substring(3,4):  " + st);

st = ss.substring();
WScript.Echo("ss.substring():  " + st);

st = ss.substring(6, 2);
WScript.Echo("ss.substring(6,2):  " + st);

st = ss.substring(-6, 2);
WScript.Echo("ss.substring(-6,2):  " + st);

st = ss.substr(-6, 2);
WScript.Echo("ss.substr(-6,2):  " + st);

st = ss.substr(-1, 4);
WScript.Echo("ss.substr(-1,4):  " + st);

//implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.substr = String.prototype.substr;
var f = obj.substr(2,3);
WScript.Echo (a);

st = ss.slice(2, 7);
WScript.Echo("ss.slice(2, 7):  " + st);

st = ss.slice();
WScript.Echo("ss.slice():  " + st);

//implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.slice = String.prototype.slice;
var f = obj.slice();
WScript.Echo (a);

WScript.Echo("Test : var ss = new String(\"abcdefg123456qweeeeaatt\");");
ss = new String("abcdefg123456qweeeeaatt");
st = ss.replace("g12", "******");
WScript.Echo("ss.replace():  " + st);

WScript.Echo("Test : var ss = new String(\"abcdefg1\" + \"23456qweeeeaatt\");");
ss = new String("abcdefg1" + "23456qweeeeaatt");
st = ss.replace("g12", "+++++");
WScript.Echo("ss.replace():  " + st);

WScript.Echo("Test : var ss = new String(\"abcdefg123456qweeeeaatt\");");
ss = new String("abcdefg123456qweeeeaatt");
st = ss.indexOf("g123");
WScript.Echo("ss.indexOf(\"g123\"):  " + st);

WScript.Echo("Test : var ss = new String(\"abcdefg1\" + \"23456qweeeeaatt\");");
ss = new String("abcdefg1" + "23456qweeeeaatt");
st = ss.indexOf("g123");
WScript.Echo("ss.indexOf(\"g123\"):  " + st);

WScript.Echo("Test : var ss = new String(\"0123456789\" + \"abcde\" + \"\" + \"fghijk\" + \"lmnoprs\");");
ss = new String("0123456789" + "abcde" + "" + "fghijk" + "lmnoprs");
st = ss.indexOf("89ab", 4);
WScript.Echo("ss.indexOf(\"89ab\", 4):  " + st);

st = ss.indexOf("def", 11);
WScript.Echo("ss.indexOf(\"def\", 11):  " + st);

st = ss.indexOf("klm", 15);
WScript.Echo("ss.indexOf(\"klm\", 15):  " + st);

WScript.Echo("Test : var ss = new String(\"0123\" + \"0123456789\" + \"\" + \"01234567\" + \"234567\");");
ss = new String("0123" + "0123456789" + "" + "01234567" + "234567");
st = ss.indexOf("0123012");
WScript.Echo("ss.indexOf(\"0123012\"):  " + st);

st = ss.indexOf("23", 1);
WScript.Echo("ss.indexOf(\"23\", 1):  " + st);

st = ss.indexOf("23", 5);
WScript.Echo("ss.indexOf(\"23\", 5):  " + st);

st = ss.indexOf("23", 10);
WScript.Echo("ss.indexOf(\"23\", 10):  " + st);

WScript.Echo("Test : var ss = new String(\"0123\" + \"0123456789\" + \"\" + \"hideundefined01234567\" + \"234567\");");
ss = new String("0123" + "0123456789" + "" + "hideundefined01234567" + "234567");
st = ss.indexOf();
WScript.Echo("ss.indexOf():  " + st);

WScript.Echo("Test : var ss = new String(\"aaccca\" + \"bbcccb\" +\"cccc\"+\"0123\" + \"0123456789\" + \"\" + \"hideundefined01234567\" + \"234567\");");
ss = new String("aaccca" + "bbcccb" + "cccc" + "0123" + "0123456789" + "" + "hideundefined01234567" + "234567");
st = ss.indexOf("6789" + "" + "hideundefined01234567" + "2345", 2);
WScript.Echo("ss.indexOf(\"6789\" + \"\" + \"hideundefined01234567\" + \"2345\", 2):  " + st);

WScript.Echo("Test : var ss = new String(\"abcdefg123456qweeeeaatt\");");
ss = new String("abcdefg123456qweeeeaatt");
st = ss.lastIndexOf("g123");
WScript.Echo("ss.lastIndexOf(\"g123\"):  " + st);

WScript.Echo("Test : var ss = new String(\"abcdefg1\" + \"23456qweeeeaatt\");");
ss = new String("abcdefg1" + "23456qweeeeaatt");
st = ss.lastIndexOf("g123");
WScript.Echo("ss.lastIndexOf(\"g123\"):  " + st);

WScript.Echo("Test : var ss = new String(\"0123456789\" + \"abcde\" + \"\" + \"fghijk\" + \"lmnoprs\");");
ss = new String("0123456789" + "abcde" + "" + "fghijk" + "lmnoprs");
st = ss.lastIndexOf("89ab", 4);
WScript.Echo("ss.lastIndexOf(\"89ab\", 4):  " + st);

st = ss.lastIndexOf("def", 18);
WScript.Echo("ss.lastIndexOf(\"def\", 18):  " + st);

st = ss.lastIndexOf("klm", 15);
WScript.Echo("ss.lastIndexOf(\"klm\", 15):  " + st);

WScript.Echo("Test : var ss = new String(\"0123\" + \"0123456789\" + \"\" + \"01234567\" + \"234567\");");
ss = new String("0123" + "0123456789" + "" + "01234567" + "234567");
st = ss.lastIndexOf("0123012");
WScript.Echo("ss.lastIndexOf(\"0123012\"):  " + st);

st = ss.lastIndexOf("23", 1);
WScript.Echo("ss.lastIndexOf(\"23\", 1):  " + st);

st = ss.lastIndexOf("23", 5);
WScript.Echo("ss.lastIndexOf(\"23\", 5):  " + st);

st = ss.lastIndexOf("23", 10);
WScript.Echo("ss.lastIndexOf(\"23\", 10):  " + st);

WScript.Echo("Test : var ss = new String(\"0123\" + \"0123456789\" + \"\" + \"hideundefined01234567\" + \"234567\");");
ss = new String("0123" + "0123456789" + "" + "hideundefined01234567" + "234567");
st = ss.lastIndexOf();
WScript.Echo("ss.lastIndexOf():  " + st);

WScript.Echo("Test : var ss = new String(\"String123456EndString\");");
var ss = new String("String123456EndString");

st = ss.search("234");
WScript.Echo("ss.search(\"234\"):  " + st);

st = ss.search(/234/);
WScript.Echo("ss.search(\/234\/):  " + st);

//implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.search = String.prototype.search;
var f = obj.search("ell");
WScript.Echo (a);

//st = ss.search(/[e-m]+/);
//WScript.Echo("ss.search(\/6.d\/):  " + st);

//st = ss.search(/\d{2,4}/);
//WScript.Echo("ss.search(\/\\d{2,4}):  " + st);

WScript.Echo("Test : var ss = new String(\"AAAAAAAAAAbbbbbbbbbb\");");
var ss = new String("AAAAAAAAAAbbbbbbbbbb");

var st = ss.toLowerCase();
WScript.Echo("ss.toLowerCase():  " + st);

WScript.Echo("Test big string");
ss = new String("AAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbb" +
"AAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbbAAAAAAAAAAbbbbbbbbbb");

st = ss.toUpperCase();
WScript.Echo("ss.toUpperCase():  " + st);

//implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.toUpperCase = String.prototype.toUpperCase;
var f = obj.toUpperCase();
WScript.Echo (a);
