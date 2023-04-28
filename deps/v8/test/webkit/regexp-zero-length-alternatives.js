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

description(
'Test regular expression processing with alternatives that match consuming no characters'
);

var emptyStr = "";
var s1 = "xxxx";
var s2 = "aaaa";
var s3 = "aax";
var s4 = "abab";
var s5 = "ab";
var s6 = "xabx";
var s7 = "g0";

// Non-capturing empty first alternative greedy '*'
var re1 = new RegExp(/(?:|a|z)*/);
shouldBe('emptyStr.match(re1)', '[""]');
shouldBe('s1.match(re1)', '[""]');
shouldBe('s2.match(re1)', '["aaaa"]');
shouldBe('s3.match(re1)', '["aa"]');

// Non-capturing empty middle alternative greedy '*'
var re2 = new RegExp(/(?:a||z)*/);
shouldBe('emptyStr.match(re2)', '[""]');
shouldBe('s1.match(re2)', '[""]');
shouldBe('s2.match(re2)', '["aaaa"]');
shouldBe('s3.match(re2)', '["aa"]');

// Non-capturing empty last alternative greedy '*'
var re3 = new RegExp(/(?:a|z|)*/);
shouldBe('emptyStr.match(re3)', '[""]');
shouldBe('s1.match(re3)', '[""]');
shouldBe('s2.match(re3)', '["aaaa"]');
shouldBe('s3.match(re3)', '["aa"]');

// Capturing empty first alternative greedy '*'
var re4 = new RegExp(/(|a|z)*/);
shouldBe('emptyStr.match(re4)', '["", undefined]');
shouldBe('s1.match(re4)', '["", undefined]');
shouldBe('s2.match(re4)', '["aaaa", "a"]');
shouldBe('s3.match(re4)', '["aa", "a"]');

// Capturing empty middle alternative greedy '*'
var re5 = new RegExp(/(a||z)*/);
shouldBe('emptyStr.match(re5)', '["", undefined]');
shouldBe('s1.match(re5)', '["", undefined]');
shouldBe('s2.match(re5)', '["aaaa", "a"]');
shouldBe('s3.match(re5)', '["aa", "a"]');

// Capturing empty last alternative greedy '*'
var re6 = new RegExp(/(a|z|)*/);
shouldBe('emptyStr.match(re6)', '["", undefined]');
shouldBe('s1.match(re6)', '["", undefined]');
shouldBe('s2.match(re6)', '["aaaa", "a"]');
shouldBe('s3.match(re6)', '["aa", "a"]');

// Non-capturing empty first alternative fixed-count
var re7 = new RegExp(/(?:|a|z){2,5}/);
shouldBe('emptyStr.match(re7)', '[""]');
shouldBe('s1.match(re7)', '[""]');
shouldBe('s2.match(re7)', '["aaa"]');
shouldBe('s3.match(re7)', '["aa"]');

// Non-capturing empty middle alternative fixed-count
var re8 = new RegExp(/(?:a||z){2,5}/);
shouldBe('emptyStr.match(re8)', '[""]');
shouldBe('s1.match(re8)', '[""]');
shouldBe('s2.match(re8)', '["aaaa"]');
shouldBe('s3.match(re8)', '["aa"]');

// Non-capturing empty last alternative fixed-count
var re9 = new RegExp(/(?:a|z|){2,5}/);
shouldBe('emptyStr.match(re9)', '[""]');
shouldBe('s1.match(re9)', '[""]');
shouldBe('s2.match(re9)', '["aaaa"]');
shouldBe('s3.match(re9)', '["aa"]');

// Non-capturing empty first alternative non-greedy '*'
var re10 = new RegExp(/(?:|a|z)*?/);
shouldBe('emptyStr.match(re10)', '[""]');
shouldBe('s1.match(re10)', '[""]');
shouldBe('s2.match(re10)', '[""]');
shouldBe('s3.match(re10)', '[""]');

// Non-capturing empty middle alternative non-greedy '*'
var re11 = new RegExp(/(?:a||z)*?/);
shouldBe('emptyStr.match(re11)', '[""]');
shouldBe('s1.match(re11)', '[""]');
shouldBe('s2.match(re11)', '[""]');
shouldBe('s3.match(re11)', '[""]');

// Non-capturing empty last alternative non-greedy '*'
var re12 = new RegExp(/(?:a|z|)*?/);
shouldBe('emptyStr.match(re12)', '[""]');
shouldBe('s1.match(re12)', '[""]');
shouldBe('s2.match(re12)', '[""]');
shouldBe('s3.match(re12)', '[""]');

// Capturing empty first alternative non-greedy '*'
var re13 = new RegExp(/(|a|z)*?/);
shouldBe('emptyStr.match(re13)', '["", undefined]');
shouldBe('s1.match(re13)', '["", undefined]');
shouldBe('s2.match(re13)', '["", undefined]');
shouldBe('s3.match(re13)', '["", undefined]');

// Capturing empty middle alternative non-greedy '*'
var re14 = new RegExp(/(a||z)*?/);
shouldBe('emptyStr.match(re14)', '["", undefined]');
shouldBe('s1.match(re14)', '["", undefined]');
shouldBe('s2.match(re14)', '["", undefined]');
shouldBe('s3.match(re14)', '["", undefined]');

// Capturing empty last alternative non-greedy '*'
var re15 = new RegExp(/(a|z|)*?/);
shouldBe('emptyStr.match(re15)', '["", undefined]');
shouldBe('s1.match(re15)', '["", undefined]');
shouldBe('s2.match(re15)', '["", undefined]');
shouldBe('s3.match(re15)', '["", undefined]');

// Non-capturing empty first alternative greedy '?'
var re16 = new RegExp(/(?:|a|z)?/);
shouldBe('emptyStr.match(re16)', '[""]');
shouldBe('s1.match(re16)', '[""]');
shouldBe('s2.match(re16)', '["a"]');
shouldBe('s3.match(re16)', '["a"]');

// Non-capturing empty middle alternative greedy '?'
var re17 = new RegExp(/(?:a||z)?/);
shouldBe('emptyStr.match(re17)', '[""]');
shouldBe('s1.match(re17)', '[""]');
shouldBe('s2.match(re17)', '["a"]');
shouldBe('s3.match(re17)', '["a"]');

// Non-capturing empty last alternative greedy '?'
var re18 = new RegExp(/(?:a|z|)?/);
shouldBe('emptyStr.match(re18)', '[""]');
shouldBe('s1.match(re18)', '[""]');
shouldBe('s2.match(re18)', '["a"]');
shouldBe('s3.match(re18)', '["a"]');

// Capturing empty first alternative greedy '?'
var re19 = new RegExp(/(|a|z)?/);
shouldBe('emptyStr.match(re19)', '["", undefined]');
shouldBe('s1.match(re19)', '["", undefined]');
shouldBe('s2.match(re19)', '["a", "a"]');
shouldBe('s3.match(re19)', '["a", "a"]');

// Capturing empty middle alternative greedy '?'
var re20 = new RegExp(/(a||z)?/);
shouldBe('emptyStr.match(re20)', '["", undefined]');
shouldBe('s1.match(re20)', '["", undefined]');
shouldBe('s2.match(re20)', '["a", "a"]');
shouldBe('s3.match(re20)', '["a", "a"]');

// Capturing empty last alternative greedy '?'
var re21 = new RegExp(/(a|z|)?/);
shouldBe('emptyStr.match(re21)', '["", undefined]');
shouldBe('s1.match(re21)', '["", undefined]');
shouldBe('s2.match(re21)', '["a", "a"]');
shouldBe('s3.match(re21)', '["a", "a"]');

// Non-capturing empty first alternative non-greedy '?'
var re22 = new RegExp(/(?:|a|z)??/);
shouldBe('emptyStr.match(re22)', '[""]');
shouldBe('s1.match(re22)', '[""]');
shouldBe('s2.match(re22)', '[""]');
shouldBe('s3.match(re22)', '[""]');

// Non-capturing empty middle alternative non-greedy '?'
var re23 = new RegExp(/(?:a||z)??/);
shouldBe('emptyStr.match(re23)', '[""]');
shouldBe('s1.match(re23)', '[""]');
shouldBe('s2.match(re23)', '[""]');
shouldBe('s3.match(re23)', '[""]');

// Non-capturing empty last alternative non-greedy '?'
var re24 = new RegExp(/(?:a|z|)??/);
shouldBe('emptyStr.match(re24)', '[""]');
shouldBe('s1.match(re24)', '[""]');
shouldBe('s2.match(re24)', '[""]');
shouldBe('s3.match(re24)', '[""]');

// Capturing empty first alternative non-greedy '?'
var re25 = new RegExp(/(|a|z)??/);
shouldBe('emptyStr.match(re25)', '["", undefined]');
shouldBe('s1.match(re25)', '["", undefined]');
shouldBe('s2.match(re25)', '["", undefined]');
shouldBe('s3.match(re25)', '["", undefined]');

// Capturing empty middle alternative non-greedy '?'
var re26 = new RegExp(/(a||z)??/);
shouldBe('emptyStr.match(re26)', '["", undefined]');
shouldBe('s1.match(re26)', '["", undefined]');
shouldBe('s2.match(re26)', '["", undefined]');
shouldBe('s3.match(re26)', '["", undefined]');

// Capturing empty last alternative non-greedy '?'
var re27 = new RegExp(/(a|z|)??/);
shouldBe('emptyStr.match(re27)', '["", undefined]');
shouldBe('s1.match(re27)', '["", undefined]');
shouldBe('s2.match(re27)', '["", undefined]');
shouldBe('s3.match(re27)', '["", undefined]');

// Non-capturing empty first alternative greedy '*' non-terminal
var re28 = new RegExp(/(?:|a|z)*x/);
shouldBe('emptyStr.match(re28)', 'null');
shouldBe('s1.match(re28)', '["x"]');
shouldBe('s2.match(re28)', 'null');
shouldBe('s3.match(re28)', '["aax"]');

// Non-capturing empty middle alternative greedy '*' non-terminal
var re29 = new RegExp(/(?:a||z)*x/);
shouldBe('emptyStr.match(re29)', 'null');
shouldBe('s1.match(re29)', '["x"]');
shouldBe('s2.match(re29)', 'null');
shouldBe('s3.match(re29)', '["aax"]');

// Non-capturing empty last alternative greedy '*' non-terminal
var re30 = new RegExp(/(?:a|z|)*x/);
shouldBe('emptyStr.match(re30)', 'null');
shouldBe('s1.match(re30)', '["x"]');
shouldBe('s2.match(re30)', 'null');
shouldBe('s3.match(re30)', '["aax"]');

// Non-capturing two possibly empty alternatives greedy '*'
var re31 = new RegExp(/(?:a*|b*)*/);
shouldBe('emptyStr.match(re31)', '[""]');
shouldBe('s1.match(re31)', '[""]');
shouldBe('s3.match(re31)', '["aa"]');
shouldBe('s4.match(re31)', '["abab"]');

// Non-capturing two possibly empty non-greedy alternatives non-greedy '*'
var re32 = new RegExp(/(?:a*?|b*?)*/);
shouldBe('emptyStr.match(re32)', '[""]');
shouldBe('s1.match(re32)', '[""]');
shouldBe('s2.match(re32)', '["aaaa"]');
shouldBe('s4.match(re32)', '["abab"]');
shouldBe('s5.match(re32)', '["ab"]');
shouldBe('s6.match(re32)', '[""]');

// Three possibly empty alternatives with greedy +
var re33 = new RegExp(/(?:(?:(?!))|g?|0*\*?)+/);
shouldBe('emptyStr.match(re33)', '[""]');
shouldBe('s1.match(re33)', '[""]');
shouldBe('s7.match(re33)', '["g0"]');

// first alternative zero length fixed count
var re34 = new RegExp(/(?:|a)/);
shouldBe('emptyStr.match(re34)', '[""]');
shouldBe('s1.match(re34)', '[""]');
shouldBe('s2.match(re34)', '[""]');
shouldBe('s3.match(re34)', '[""]');
