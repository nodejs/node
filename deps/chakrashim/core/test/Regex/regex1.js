//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var groups = {};

function Assert(condition, category)
{
    if (!groups[category]) {
        groups[category] = 1;
    } else {
        groups[category]++;
    }

    if (!condition) {
        write(category + " test " + groups[category] + " failed");
    } else {
        write(category + " test " + groups[category] + " passed");
    }
}

write("regex test1");

var re = /a/;
var str = new String("abcda");
if (re.test(str)) {
    write("  regex.test pass");
}
else {
    write(" regex.test fail");
}

if (str.match(re)) {
    write("  string.match pass");
}
else {
    write(" string.match fail");
}
var array = re.exec(str);
write(" string.exec : " + array);


var s = "";
var a = s.split(/\s+/);
write("a.length : " + a.length);
write("a[0]:" + a[0]);

var reTemp = /abc/i;
var re = new RegExp(reTemp, "g");
var tmp = "abcdef".replace(re, "");
Assert(re.lastIndex == 0, "lastIndex");

var re = new RegExp(/abc/i, "g");
var tmp = "abcdef".match(re);
Assert(re.lastIndex == 0, "lastIndex");

var re = new RegExp(/abc/g);
re.exec("abcdef");
Assert(re.lastIndex == 3, "lastIndex");

var re = /abc/;
re.exec("abcdef");
Assert(re.lastIndex == 0, "lastIndex");

var re = new RegExp(/abc/g, "i");
Assert(re.global == false, "global");
Assert(re.ignoreCase == true, "ignoreCase");

var re = /abc/i;
var re1 = new RegExp(re, "gm");
Assert(re.global == false, "global");
Assert(re.multiline == false, "multiline");
Assert(re.ignoreCase == true, "ignoreCase");
Assert(re1.global == true, "global");
Assert(re1.multiline == true, "multiline");
Assert(re1.ignoreCase == false, "ignoreCase");

var exceptionThrown = false;
try 
{
    var re = new RegExp(/a/g, "ab");
} 
catch (ex) 
{
    exceptionThrown = true;
}
Assert(exceptionThrown, "invalid flags");

var re = /(ab)/g

"abc     ".match(re);
Assert(RegExp.$1 == "ab", "lastIndex");

var re = /test/;
var exceptionThrown = false;
try
{
    re.lastIndex = { toString: function() { throw "an exception string"; } }
}
catch (ex)
{
    exceptionThrown = true;
}

Assert(exceptionThrown == false, "lastIndex");

exceptionThrown = false;

try
{
    Write("LastIndex is " + re.lastIndex);
}
catch (ex)
{
    exceptionThrown = true;
}

Assert(exceptionThrown == true, "lastIndex");

function testsc(r, s) {
    if (!r.test(s))
        write("invalid interpretation of '" + r + "'");
}

testsc(/^\cA$/, "\x01");
testsc(/^[\cA]$/, "\x01");
testsc(/^\c1$/, "\\c1");
testsc(/^\c$/, "\\c");
testsc(/\c/, "\\c");
testsc(/^\c\1$/, "\\c\x01");
testsc(/\c/, "\\c");
testsc(/^[\c1]$/, "\x11");
testsc(/^[\c]$/, "c");
testsc(/^[\c]]$/, "c]");
testsc(/^[\c-e]+$/, "cde");

//Octal handling
testsc(/^\777$/, "\x3F7");
testsc(/^\777$/, "\777");
testsc(/^\170$/, "x");

//Octal handling test for values > 127
c=[/[\300-\306]/g,"A",/[\340-\346]/g,"a",/\307/g,"C",/\347/g,"c",/[\310-\313]/g,"E",/[\350-\353]/g,"e",/[\314-\317]/g,"I",/[\354-\357]/g,"i",/\321/g,"N",/\361/g,"n",/[\322-\330]/g,"O",/[\362-\370]/g,"o",/[\331-\334]/g,"U",/[\371-\374]/g,"u"];

//Negation of empty char set [^] test
write("aa".match(/([^])(\1)/));


