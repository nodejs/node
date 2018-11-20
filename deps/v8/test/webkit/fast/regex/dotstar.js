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

var regexp1 = /.*blah.*/;
shouldBeNull("regexp1.exec('test')");
shouldBe("regexp1.exec('blah')", "['blah']");
shouldBe("regexp1.exec('1blah')", "['1blah']");
shouldBe("regexp1.exec('blah1')", "['blah1']");
shouldBe("regexp1.exec('blah blah blah')", "['blah blah blah']");
shouldBe("regexp1.exec('blah\\nsecond')", "['blah']");
shouldBe("regexp1.exec('first\\nblah')", "['blah']");
shouldBe("regexp1.exec('first\\nblah\\nthird')", "['blah']");
shouldBe("regexp1.exec('first\\nblah2\\nblah3')", "['blah2']");

var regexp2 = /^.*blah.*/;
shouldBeNull("regexp2.exec('test')");
shouldBe("regexp2.exec('blah')", "['blah']");
shouldBe("regexp2.exec('1blah')", "['1blah']");
shouldBe("regexp2.exec('blah1')", "['blah1']");
shouldBe("regexp2.exec('blah blah blah')", "['blah blah blah']");
shouldBe("regexp2.exec('blah\\nsecond')", "['blah']");
shouldBeNull("regexp2.exec('first\\nblah')");
shouldBeNull("regexp2.exec('first\\nblah\\nthird')");
shouldBeNull("regexp2.exec('first\\nblah2\\nblah3')");

var regexp3 = /.*blah.*$/;
shouldBeNull("regexp3.exec('test')");
shouldBe("regexp3.exec('blah')", "['blah']");
shouldBe("regexp3.exec('1blah')", "['1blah']");
shouldBe("regexp3.exec('blah1')", "['blah1']");
shouldBe("regexp3.exec('blah blah blah')", "['blah blah blah']");
shouldBeNull("regexp3.exec('blah\\nsecond')");
shouldBe("regexp3.exec('first\\nblah')", "['blah']");
shouldBeNull("regexp3.exec('first\\nblah\\nthird')");
shouldBe("regexp3.exec('first\\nblah2\\nblah3')", "['blah3']");

var regexp4 = /^.*blah.*$/;
shouldBeNull("regexp4.exec('test')");
shouldBe("regexp4.exec('blah')", "['blah']");
shouldBe("regexp4.exec('1blah')", "['1blah']");
shouldBe("regexp4.exec('blah1')", "['blah1']");
shouldBe("regexp4.exec('blah blah blah')", "['blah blah blah']");
shouldBeNull("regexp4.exec('blah\\nsecond')");
shouldBeNull("regexp4.exec('first\\nblah')");
shouldBeNull("regexp4.exec('first\\nblah\\nthird')");
shouldBeNull("regexp4.exec('first\\nblah2\\nblah3')");

var regexp5 = /.*?blah.*/;
shouldBeNull("regexp5.exec('test')");
shouldBe("regexp5.exec('blah')", "['blah']");
shouldBe("regexp5.exec('1blah')", "['1blah']");
shouldBe("regexp5.exec('blah1')", "['blah1']");
shouldBe("regexp5.exec('blah blah blah')", "['blah blah blah']");
shouldBe("regexp5.exec('blah\\nsecond')", "['blah']");
shouldBe("regexp5.exec('first\\nblah')", "['blah']");
shouldBe("regexp5.exec('first\\nblah\\nthird')", "['blah']");
shouldBe("regexp5.exec('first\\nblah2\\nblah3')", "['blah2']");

var regexp6 = /.*blah.*?/;
shouldBeNull("regexp6.exec('test')");
shouldBe("regexp6.exec('blah')", "['blah']");
shouldBe("regexp6.exec('1blah')", "['1blah']");
shouldBe("regexp6.exec('blah1')", "['blah']");
shouldBe("regexp6.exec('blah blah blah')", "['blah blah blah']");
shouldBe("regexp6.exec('blah\\nsecond')", "['blah']");
shouldBe("regexp6.exec('first\\nblah')", "['blah']");
shouldBe("regexp6.exec('first\\nblah\\nthird')", "['blah']");
shouldBe("regexp6.exec('first\\nblah2\\nblah3')", "['blah']");

var regexp7 = /^.*?blah.*?$/;
shouldBeNull("regexp7.exec('test')");
shouldBe("regexp7.exec('blah')", "['blah']");
shouldBe("regexp7.exec('1blah')", "['1blah']");
shouldBe("regexp7.exec('blah1')", "['blah1']");
shouldBe("regexp7.exec('blah blah blah')", "['blah blah blah']");
shouldBeNull("regexp7.exec('blah\\nsecond')");
shouldBeNull("regexp7.exec('first\\nblah')");
shouldBeNull("regexp7.exec('first\\nblah\\nthird')");
shouldBeNull("regexp7.exec('first\\nblah2\\nblah3')");

var regexp8 = /^(.*)blah.*$/;
shouldBeNull("regexp8.exec('test')");
shouldBe("regexp8.exec('blah')", "['blah','']");
shouldBe("regexp8.exec('1blah')", "['1blah','1']");
shouldBe("regexp8.exec('blah1')", "['blah1','']");
shouldBe("regexp8.exec('blah blah blah')", "['blah blah blah','blah blah ']");
shouldBeNull("regexp8.exec('blah\\nsecond')");
shouldBeNull("regexp8.exec('first\\nblah')");
shouldBeNull("regexp8.exec('first\\nblah\\nthird')");
shouldBeNull("regexp8.exec('first\\nblah2\\nblah3')");

var regexp9 = /.*blah.*/m;
shouldBeNull("regexp9.exec('test')");
shouldBe("regexp9.exec('blah')", "['blah']");
shouldBe("regexp9.exec('1blah')", "['1blah']");
shouldBe("regexp9.exec('blah1')", "['blah1']");
shouldBe("regexp9.exec('blah blah blah')", "['blah blah blah']");
shouldBe("regexp9.exec('blah\\nsecond')", "['blah']");
shouldBe("regexp9.exec('first\\nblah')", "['blah']");
shouldBe("regexp9.exec('first\\nblah\\nthird')", "['blah']");
shouldBe("regexp9.exec('first\\nblah2\\nblah3')", "['blah2']");

var regexp10 = /^.*blah.*/m;
shouldBeNull("regexp10.exec('test')");
shouldBe("regexp10.exec('blah')", "['blah']");
shouldBe("regexp10.exec('1blah')", "['1blah']");
shouldBe("regexp10.exec('blah1')", "['blah1']");
shouldBe("regexp10.exec('blah blah blah')", "['blah blah blah']");
shouldBe("regexp10.exec('blah\\nsecond')", "['blah']");
shouldBe("regexp10.exec('first\\nblah')", "['blah']");
shouldBe("regexp10.exec('first\\nblah\\nthird')", "['blah']");
shouldBe("regexp10.exec('first\\nblah2\\nblah3')", "['blah2']");

var regexp11 = /.*(?:blah).*$/;
shouldBeNull("regexp11.exec('test')");
shouldBe("regexp11.exec('blah')", "['blah']");
shouldBe("regexp11.exec('1blah')", "['1blah']");
shouldBe("regexp11.exec('blah1')", "['blah1']");
shouldBe("regexp11.exec('blah blah blah')", "['blah blah blah']");
shouldBeNull("regexp11.exec('blah\\nsecond')");
shouldBe("regexp11.exec('first\\nblah')", "['blah']");
shouldBeNull("regexp11.exec('first\\nblah\\nthird')");
shouldBe("regexp11.exec('first\\nblah2\\nblah3')", "['blah3']");

var regexp12 = /.*(?:blah|buzz|bang).*$/;
shouldBeNull("regexp12.exec('test')");
shouldBe("regexp12.exec('blah')", "['blah']");
shouldBe("regexp12.exec('1blah')", "['1blah']");
shouldBe("regexp12.exec('blah1')", "['blah1']");
shouldBe("regexp12.exec('blah blah blah')", "['blah blah blah']");
shouldBeNull("regexp12.exec('blah\\nsecond')");
shouldBe("regexp12.exec('first\\nblah')", "['blah']");
shouldBeNull("regexp12.exec('first\\nblah\\nthird')");
shouldBe("regexp12.exec('first\\nblah2\\nblah3')", "['blah3']");

var regexp13 = /.*\n\d+.*/;
shouldBe("regexp13.exec('abc\\n123')", "['abc\\n123']");
