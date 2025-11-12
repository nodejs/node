// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("KDE JS Test");
shouldBe("(new RegExp()).source", "'(?:)'");
shouldBe("Boolean(new RegExp())", "true");
shouldBeTrue("isNaN(Number(new RegExp()))");

// RegExp constructor called as a function
shouldBe("RegExp(/x/).source", "'x'");
//shouldBe("RegExp(/x/, 'g').source", "'/x/'"); // can't supply flags when constructing one RegExp from another, says mozilla
shouldBe("RegExp('x', 'g').global", "true");
shouldBe("RegExp('x').source", "'x'");

// RegExp constructor
shouldBe("new RegExp('x').source", "'x'");

var ri = /a/i;
var rm = /a/m;
var rg = /a/g;

shouldBeFalse("(/a/).global");
shouldBe("typeof (/a/).global", "'boolean'");
shouldBeTrue("rg.global");
shouldBeFalse("(/a/).ignoreCase");
shouldBeTrue("ri.ignoreCase");
shouldBeFalse("(/a/).multiline");
shouldBeTrue("rm.multiline");
shouldBe("rg.toString()", "'/a/g'");
shouldBe("ri.toString()", "'/a/i'");
shouldBe("rm.toString()", "'/a/m'");

// check properety attributes
rg.global = false;
shouldBeTrue("rg.global");
ri.ignoreCase = false;
shouldBeTrue("ri.ignoreCase");
rm.multiline = false;
shouldBeTrue("rm.multiline");

shouldBe("Boolean(/a/.test)", "true");
shouldBe("/(b)c/.exec('abcd').toString()", "\"bc,b\"");
shouldBe("/(b)c/.exec('abcd').length", "2");
shouldBe("/(b)c/.exec('abcd').index", "1");
shouldBe("/(b)c/.exec('abcd').input", "'abcd'");

var rs = /foo/;
rs.source = "bar";
shouldBe("rs.source", "'foo'");

shouldBe("var r = new RegExp(/x/); r.global=true; r.lastIndex = -1; typeof r.test('a')", "'boolean'");

shouldBe("'abcdefghi'.match(/(abc)def(ghi)/).toString()","'abcdefghi,abc,ghi'");
shouldBe("/(abc)def(ghi)/.exec('abcdefghi').toString()","'abcdefghi,abc,ghi'");
shouldBe("RegExp.$1","'abc'");
shouldBe("RegExp.$2","'ghi'");
shouldBe("RegExp.$3","''");

shouldBe("'abcdefghi'.match(/(a(b(c(d(e)f)g)h)i)/).toString()", "'abcdefghi,abcdefghi,bcdefgh,cdefg,def,e'");
shouldBe("RegExp.$1","'abcdefghi'");
shouldBe("RegExp.$2","'bcdefgh'");
shouldBe("RegExp.$3","'cdefg'");
shouldBe("RegExp.$4","'def'");
shouldBe("RegExp.$5","'e'");
shouldBe("RegExp.$6","''");

shouldBe("'(100px 200px 150px 15px)'.match(/\\((\\d+)(px)* (\\d+)(px)* (\\d+)(px)* (\\d+)(px)*\\)/).toString()","'(100px 200px 150px 15px),100,px,200,px,150,px,15,px'");
shouldBe("RegExp.$1","'100'");
shouldBe("RegExp.$3","'200'");
shouldBe("RegExp.$5","'150'");
shouldBe("RegExp.$7","'15'");
shouldBe("''.match(/\((\\d+)(px)* (\\d+)(px)* (\\d+)(px)* (\\d+)(px)*\)/)","null");
// After a failed match, cached results on the RegExp object are unchanged.
shouldBe("RegExp.$1","'100'");
shouldBe("RegExp.$3","'200'");
shouldBe("RegExp.$5","'150'");
shouldBe("RegExp.$7","'15'");

var invalidChars = /[^@\.\w]/g; // #47092
shouldBe("'faure@kde.org'.match(invalidChars) == null", "true");
shouldBe("'faure-kde@kde.org'.match(invalidChars) == null", "false");

shouldBe("'test1test2'.replace('test','X')","'X1test2'");
shouldBe("'test1test2'.replace(/\\d/,'X')","'testXtest2'");
shouldBe("'1test2test3'.replace(/\\d/,'')","'test2test3'");
shouldBe("'test1test2'.replace(/test/g,'X')","'X1X2'");
shouldBe("'1test2test3'.replace(/\\d/g,'')","'testtest'");
shouldBe("'1test2test3'.replace(/x/g,'')","'1test2test3'");
shouldBe("'test1test2'.replace(/(te)(st)/g,'$2$1')","'stte1stte2'");
shouldBe("'foo+bar'.replace(/\\+/g,'%2B')", "'foo%2Bbar'");
var caught = false; try { new RegExp("+"); } catch (e) { caught = true; }
shouldBeTrue("caught"); // #40435
shouldBe("'foo'.replace(/z?/g,'x')", "'xfxoxox'");
shouldBe("'test test'.replace(/\\s*/g,'')","'testtest'"); // #50985
shouldBe("'abc$%@'.replace(/[^0-9a-z]*/gi,'')","'abc'"); // #50848
shouldBe("'ab'.replace(/[^\\d\\.]*/gi,'')","''"); // #75292
shouldBe("'1ab'.replace(/[^\\d\\.]*/gi,'')","'1'"); // #75292

shouldBe("'1test2test3blah'.split(/test/).toString()","'1,2,3blah'");
var reg = /(\d\d )/g;
var str = new String('98 76 blah');
shouldBe("reg.exec(str).toString()","'98 ,98 '");
shouldBe("reg.lastIndex","3");
shouldBe("RegExp.$1","'98 '");
shouldBe("RegExp.$2","''");

shouldBe("reg.exec(str).toString()","'76 ,76 '");
shouldBe("reg.lastIndex","6");
shouldBe("RegExp.$1","'76 '");
shouldBe("RegExp.$2","''");

shouldBe("reg.exec(str)","null");
shouldBe("reg.lastIndex","0");

// http://www.devguru.com/Technologies/ecmascript/quickref/regexp_lastindex.html
// Looks IE-only though
//shouldBe( "var re=/ships*\s/; re.exec('the hardships of traveling'); re.lastIndex", "14" );
//shouldBe( "var re=/ships*\s/; str='the hardships of traveling'; re.exec(str); re.exec(str); re.lastIndex", "0" );

// http://devedge.netscape.com/library/manuals/2000/javascript/1.5/guide/regexp.html
shouldBe( "myRe=/d(b+)d/g; myArray = myRe.exec('cdbbdbsbz'); myRe.lastIndex", "5" );

reg = /^u/i;
shouldBeTrue("reg.ignoreCase == true");
shouldBeTrue("reg.global === false");
shouldBeTrue("reg.multiline === false");
shouldBeTrue("reg.test('UGO')");

// regexp are writable ?
shouldBe("reg.x = 1; reg.x", "1");
// shared data ?
shouldBe("var r2 = reg; r2.x = 2; reg.x", "2");

var str = new String("For more information, see Chapter 3.4.5.1");
re = /(chapter \d+(\.\d)*)/i;
// This returns the array containing Chapter 3.4.5.1,Chapter 3.4.5.1,.1
// 'Chapter 3.4.5.1' is the first match and the first value remembered from (Chapter \d+(\.\d)*).
// '.1' is the second value remembered from (\.\d)
shouldBe("str.match(re).toString()","'Chapter 3.4.5.1,Chapter 3.4.5.1,.1'");

str = "abcDdcba";
// The returned array contains D, d.
shouldBe("str.match(/d/gi).toString()","'D,d'");

// unicode escape sequence
shouldBe("/\\u0061/.source", "'\\\\u0061'");
shouldBe("'abc'.match(/\\u0062/).toString()", "'b'");

shouldBe("Object.prototype.toString.apply(RegExp.prototype)",
         "'[object RegExp]'");

// not sure what this should return. most importantly
// it doesn't throw an exception
shouldBe("typeof RegExp.prototype.toString()", "'string'");

// Empty regular expressions have string representation /(?:)/
shouldBe("new RegExp().toString()", "'/(?:)/'");
shouldBe("(new RegExp('(?:)')).source", "'(?:)'");
shouldBe("/(?:)/.toString()", "'/(?:)/'");
shouldBe("/(?:)/.source", "'(?:)'");

debug("Done.");
