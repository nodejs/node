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

description("This page tests handling of parentheses subexpressions.");

var regexp1 = /(a|A)(b|B)/;
shouldBe("regexp1.exec('abc')", "['ab','a','b']");

var regexp2 = /(a((b)|c|d))e/;
shouldBe("regexp2.exec('abacadabe')", "['abe','ab','b','b']");

var regexp3 = /(a(b|(c)|d))e/;
shouldBe("regexp3.exec('abacadabe')", "['abe','ab','b',undefined]");

var regexp4 = /(a(b|c|(d)))e/;
shouldBe("regexp4.exec('abacadabe')", "['abe','ab','b',undefined]");

var regexp5 = /(a((b)|(c)|(d)))e/;
shouldBe("regexp5.exec('abacadabe')", "['abe','ab','b','b',undefined,undefined]");

var regexp6 = /(a((b)|(c)|(d)))/;
shouldBe("regexp6.exec('abcde')", "['ab','ab','b','b',undefined,undefined]");

var regexp7 = /(a(b)??)??c/;
shouldBe("regexp7.exec('abc')", "['abc','ab','b']");

var regexp8 = /(a|(e|q))(x|y)/;
shouldBe("regexp8.exec('bcaddxqy')" , "['qy','q','q','y']");

var regexp9 = /((t|b)?|a)$/;
shouldBe("regexp9.exec('asdfjejgsdflaksdfjkeljghkjea')", "['a','a',undefined]");

var regexp10 = /(?:h|e?(?:t|b)?|a?(?:t|b)?)(?:$)/;
shouldBe("regexp10.exec('asdfjejgsdflaksdfjkeljghat')", "['at']");

var regexp11 = /([Jj]ava([Ss]cript)?)\sis\s(fun\w*)/;
shouldBeNull("regexp11.exec('Developing with JavaScript is dangerous, do not try it without assistance')");

var regexp12 = /(?:(.+), )?(.+), (..) to (?:(.+), )?(.+), (..)/;
shouldBe("regexp12.exec('Seattle, WA to Buckley, WA')", "['Seattle, WA to Buckley, WA', undefined, 'Seattle', 'WA', undefined, 'Buckley', 'WA']");

var regexp13 = /(A)?(A.*)/;
shouldBe("regexp13.exec('zxcasd;fl\ ^AaaAAaaaf;lrlrzs')", "['AaaAAaaaf;lrlrzs',undefined,'AaaAAaaaf;lrlrzs']");

var regexp14 = /(a)|(b)/;
shouldBe("regexp14.exec('b')", "['b',undefined,'b']");

var regexp15 = /^(?!(ab)de|x)(abd)(f)/;
shouldBe("regexp15.exec('abdf')", "['abdf',undefined,'abd','f']");

var regexp16 = /(a|A)(b|B)/;
shouldBe("regexp16.exec('abc')", "['ab','a','b']");

var regexp17 = /(a|d|q|)x/i;
shouldBe("regexp17.exec('bcaDxqy')", "['Dx','D']");

var regexp18 = /^.*?(:|$)/;
shouldBe("regexp18.exec('Hello: World')", "['Hello:',':']");

var regexp19 = /(ab|^.{0,2})bar/;
shouldBe("regexp19.exec('barrel')", "['bar','']");

var regexp20 = /(?:(?!foo)...|^.{0,2})bar(.*)/;
shouldBe("regexp20.exec('barrel')", "['barrel','rel']");
shouldBe("regexp20.exec('2barrel')", "['2barrel','rel']");

var regexp21 = /([a-g](b|B)|xyz)/;
shouldBe("regexp21.exec('abc')", "['ab','ab','b']");

var regexp22 = /(?:^|;)\s*abc=([^;]*)/;
shouldBeNull("regexp22.exec('abcdlskfgjdslkfg')");

var regexp23 = /"[^<"]*"|'[^<']*'/;
shouldBe("regexp23.exec('<html xmlns=\"http://www.w3.org/1999/xhtml\"')", "['\"http://www.w3.org/1999/xhtml\"']");

var regexp24 = /^(?:(?=abc)\w{3}:|\d\d)$/;
shouldBeNull("regexp24.exec('123')");

var regexp25 = /^\s*(\*|[\w\-]+)(\b|$)?/;
shouldBe("regexp25.exec('this is a test')", "['this','this',undefined]");
shouldBeNull("regexp25.exec('!this is a test')");

var regexp26 = /a(b)(a*)|aaa/;
shouldBe("regexp26.exec('aaa')", "['aaa',undefined,undefined]");

var regexp27 = new RegExp(
    "^" +
    "(?:" +
        "([^:/?#]+):" + /* scheme */
    ")?" +
    "(?:" +
        "(//)" + /* authorityRoot */
        "(" + /* authority */
            "(?:" +
                "(" + /* userInfo */
                    "([^:@]*)" + /* user */
                    ":?" +
                    "([^:@]*)" + /* password */
                ")?" +
                "@" +
            ")?" +
            "([^:/?#]*)" + /* domain */
            "(?::(\\d*))?" + /* port */
        ")" +
    ")?" +
    "([^?#]*)" + /*path*/
    "(?:\\?([^#]*))?" + /* queryString */
    "(?:#(.*))?" /*fragment */
);
shouldBe("regexp27.exec('file:///Users/Someone/Desktop/HelloWorld/index.html')", "['file:///Users/Someone/Desktop/HelloWorld/index.html','file','//','',undefined,undefined,undefined,'',undefined,'/Users/Someone/Desktop/HelloWorld/index.html',undefined,undefined]");

var regexp28 = new RegExp(
    "^" +
    "(?:" +
        "([^:/?#]+):" + /* scheme */
    ")?" +
    "(?:" +
        "(//)" + /* authorityRoot */
        "(" + /* authority */
            "(" + /* userInfo */
                "([^:@]*)" + /* user */
                ":?" +
                "([^:@]*)" + /* password */
            ")?" +
            "@" +
        ")" +
    ")?"
);
shouldBe("regexp28.exec('file:///Users/Someone/Desktop/HelloWorld/index.html')", "['file:','file',undefined,undefined,undefined,undefined,undefined]");

var regexp29 = /^\s*((\[[^\]]+\])|(u?)("[^"]+"))\s*/;
shouldBeNull("regexp29.exec('Committer:')");

var regexp30 = /^\s*((\[[^\]]+\])|m(u?)("[^"]+"))\s*/;
shouldBeNull("regexp30.exec('Committer:')");

var regexp31 = /^\s*(m(\[[^\]]+\])|m(u?)("[^"]+"))\s*/;
shouldBeNull("regexp31.exec('Committer:')");

var regexp32 = /\s*(m(\[[^\]]+\])|m(u?)("[^"]+"))\s*/;
shouldBeNull("regexp32.exec('Committer:')");

var regexp33 = RegExp('^(?:(?:(a)(xyz|[^>"\'\s]*)?)|(/?>)|.[^\w\s>]*)');
shouldBe("regexp33.exec('> <head>')","['>',undefined,undefined,'>']");

var regexp34 = /(?:^|\b)btn-\S+/;
shouldBeNull("regexp34.exec('xyz123')");
shouldBe("regexp34.exec('btn-abc')","['btn-abc']");
shouldBeNull("regexp34.exec('btn- abc')");
shouldBeNull("regexp34.exec('XXbtn-abc')");
shouldBe("regexp34.exec('XX btn-abc')","['btn-abc']");

var regexp35 = /^((a|b)(x|xxx)|)$/;
shouldBe("regexp35.exec('ax')", "['ax','ax','a','x']");
shouldBeNull("regexp35.exec('axx')");
shouldBe("regexp35.exec('axxx')", "['axxx','axxx','a','xxx']");
shouldBe("regexp35.exec('bx')", "['bx','bx','b','x']");
shouldBeNull("regexp35.exec('bxx')");
shouldBe("regexp35.exec('bxxx')", "['bxxx','bxxx','b','xxx']");

var regexp36 = /^((\/|\.|\-)(\d\d|\d\d\d\d)|)$/;
shouldBe("regexp36.exec('/2011')", "['/2011','/2011','/','2011']");
shouldBe("regexp36.exec('/11')", "['/11','/11','/','11']");
shouldBeNull("regexp36.exec('/123')");

var regexp37 = /^([1][0-2]|[0]\d|\d)(\/|\.|\-)([0-2]\d|[3][0-1]|\d)((\/|\.|\-)(\d\d|\d\d\d\d)|)$/;
shouldBe("regexp37.exec('7/4/1776')", "['7/4/1776','7','/','4','/1776','/','1776']");
shouldBe("regexp37.exec('07-04-1776')", "['07-04-1776','07','-','04','-1776','-','1776']");

var regexp38 = /^(z|(x|xx)|b|)$/;
shouldBe("regexp38.exec('xx')", "['xx','xx','xx']");
shouldBe("regexp38.exec('b')", "['b','b',undefined]");
shouldBe("regexp38.exec('z')", "['z','z',undefined]");
shouldBe("regexp38.exec('')", "['','',undefined]");

var regexp39 = /(8|((?=P)))?/;
shouldBe("regexp39.exec('')", "['',undefined,undefined]");
shouldBe("regexp39.exec('8')", "['8','8',undefined]");
shouldBe("regexp39.exec('zP')", "['',undefined,undefined]");

var regexp40 = /((8)|((?=P){4}))?()/;
shouldBe("regexp40.exec('')", "['',undefined,undefined,undefined,'']");
shouldBe("regexp40.exec('8')", "['8','8','8',undefined,'']");
shouldBe("regexp40.exec('zPz')", "['',undefined,undefined,undefined,'']");
shouldBe("regexp40.exec('zPPz')", "['',undefined,undefined,undefined,'']");
shouldBe("regexp40.exec('zPPPz')", "['',undefined,undefined,undefined,'']");
shouldBe("regexp40.exec('zPPPPz')", "['',undefined,undefined,undefined,'']");

var regexp41 = /(([\w\-]+:\/\/?|www[.])[^\s()<>]+(?:([\w\d]+)|([^\[:punct:\]\s()<>\W]|\/)))/;
shouldBe("regexp41.exec('Here is a link: http://www.acme.com/our_products/index.html. That is all we want!')", "['http://www.acme.com/our_products/index.html','http://www.acme.com/our_products/index.html','http://','l',undefined]");

var regexp42 = /((?:(4)?))?/;
shouldBe("regexp42.exec('')", "['',undefined,undefined]");
shouldBe("regexp42.exec('4')", "['4','4','4']");
shouldBe("regexp42.exec('4321')", "['4','4','4']");

shouldBeTrue("/(?!(?=r{0}){2,})|((z)?)?/gi.test('')");

var regexp43 = /(?!(?:\1+s))/;
shouldBe("regexp43.exec('SSS')", "['']");

var regexp44 = /(?!(?:\3+(s+?)))/g;
shouldBe("regexp44.exec('SSS')", "['',undefined]");

var regexp45 = /((?!(?:|)v{2,}|))/;
shouldBeNull("regexp45.exec('vt')");

var regexp46 = /(w)(?:5{3}|())|pk/;
shouldBeNull("regexp46.exec('5')");
shouldBe("regexp46.exec('pk')", "['pk',undefined,undefined]");
shouldBe("regexp46.exec('Xw555')", "['w555','w',undefined]");
shouldBe("regexp46.exec('Xw55pk5')", "['w','w','']");

var regexp47 = /(.*?)(?:(?:\?(.*?)?)?)(?:(?:#)?)$/;
shouldBe("regexp47.exec('/www.acme.com/this/is/a/path/file.txt')", "['/www.acme.com/this/is/a/path/file.txt','/www.acme.com/this/is/a/path/file.txt',undefined]");

var regexp48 = /^(?:(\w+):\/*([\w\.\-\d]+)(?::(\d+)|)(?=(?:\/|$))|)(?:$|\/?(.*?)(?:\?(.*?)?|)(?:#(.*)|)$)/;
/* The regexp on the prior line confuses Xcode syntax highlighting, this coment fixes it! */
shouldBe("regexp48.exec('http://www.acme.com/this/is/a/path/file.txt')", "['http://www.acme.com/this/is/a/path/file.txt','http','www.acme.com',undefined,'this/is/a/path/file.txt',undefined,undefined]");

var regexp49 = /(?:([^:]*?)(?:(?:\?(.*?)?)?)(?:(?:#)?)$)|(?:^(?:(\w+):\/*([\w\.\-\d]+)(?::(\d+)|)(?=(?:\/|$))|)(?:$|\/?(.*?)(?:\?(.*?)?|)(?:#(.*)|)$))/;
/* The regexp on the prior line confuses Xcode syntax highlighting, this coment fixes it! */
shouldBe("regexp49.exec('http://www.acme.com/this/is/a/path/file.txt')", "['http://www.acme.com/this/is/a/path/file.txt',undefined,undefined,'http','www.acme.com',undefined,'this/is/a/path/file.txt',undefined,undefined]");

var regexp50 = /((a)b{28,}c|d)x/;
shouldBeNull("regexp50.exec('((a)b{28,}c|d)x')");
shouldBe("regexp50.exec('abbbbbbbbbbbbbbbbbbbbbbbbbbbbcx')", "['abbbbbbbbbbbbbbbbbbbbbbbbbbbbcx', 'abbbbbbbbbbbbbbbbbbbbbbbbbbbbc', 'a']");
shouldBe("regexp50.exec('dx')", "['dx', 'd', undefined]");

var s = "((.\s{-}).{28,}\P{Yi}?{,30}\|.)\x9e{-,}\P{Any}";
var regexp51 = new RegExp(s);
shouldBeNull("regexp51.exec('abc')");
shouldBe("regexp51.exec(s)", "[')\x9e{-,}P{Any}',')',undefined]");

var regexp52 = /(Rob)|(Bob)|(Robert)|(Bobby)/;
shouldBe("'Hi Bob'.match(regexp52)", "['Bob',undefined,'Bob',undefined,undefined]");

// Test cases discovered by fuzzing that crashed the compiler.
var regexp53 = /(?=(?:(?:(gB)|(?!cs|<))((?=(?!v6){0,})))|(?=#)+?)/m;
shouldBe("regexp53.exec('#')", "['',undefined,'']");
var regexp54 = /((?:(?:()|(?!))((?=(?!))))|())/m;
shouldBe("regexp54.exec('#')", "['','',undefined,undefined,'']");
var regexp55 = /(?:(?:(?:a?|(?:))((?:)))|a?)/m;
shouldBe("regexp55.exec('#')", "['','']");

// Test evaluation order of empty subpattern alternatives.
var regexp56 = /(|a)/;
shouldBe("regexp56.exec('a')", "['','']");
var regexp57 = /(a|)/;
shouldBe("regexp57.exec('a')", "['a','a']");

// Tests that non-greedy repeat quantified parentheses will backtrack through multiple frames of subpattern matches.
var regexp58 = /a|b(?:[^b])*?c/;
shouldBe("regexp58.exec('badbc')", "['a']");
var regexp59 = /(X(?:.(?!X))*?Y)|(Y(?:.(?!Y))*?Z)/g;
shouldBe("'Y aaa X Match1 Y aaa Y Match2 Z'.match(regexp59)", "['X Match1 Y','Y Match2 Z']");
