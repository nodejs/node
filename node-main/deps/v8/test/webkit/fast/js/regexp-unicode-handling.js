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

'Test for proper handling of Unicode RegExps and <a href="http://bugzilla.webkit.org/show_bug.cgi?id=7445">bug 7445</a>: Gmail puts wrong subject in replies.'

);

// Regex to match Re in various languanges straight from Gmail source
var I3=/^\s*(fwd|re|aw|antw|antwort|wg|sv|ang|odp|betreff|betr|transf|reenv\.|reenv|in|res|resp|resp\.|enc|\u8f6c\u53d1|\u56DE\u590D|\u041F\u0435\u0440\u0435\u0441\u043B|\u041E\u0442\u0432\u0435\u0442):\s*(.*)$/i;

// Other RegExs from Gmail source
var Ci=/\s+/g;
var BC=/^ /;
var BG=/ $/;

// Strips leading Re or similar (from Gmail source)
function cy(a) {
    //var b = I3.exec(a);
    var b = I3.exec(a);

    if (b) {
        a = b[2];
    }

    return Gn(a);
}

// This function replaces consecutive whitespace with a single space
// then removes a leading and trailing space if they exist. (From Gmail)
function Gn(a) {
    return a.replace(Ci, " ").replace(BC, "").replace(BG, "");
}

shouldBe("cy('Re: Moose')", "'Moose'")
shouldBe("cy('\\u8f6c\\u53d1: Moose')", "'Moose'")

// Test handling of \u2820 (skull and crossbones)
var sample="sample bm\u2820p cm\\u2820p";

var inlineRe=/.m\u2820p/
var evalInlineRe=eval("/.m\\u2820p/")
var explicitRe=new RegExp(".m\\u2820p")
var newFromInlineRe=new RegExp(inlineRe.source)
var evalFromInlineRe=eval(inlineRe.toString())
var newFromEvalInlineRe=new RegExp(evalInlineRe.source)
var evalFromEvalInlineRe=eval(evalInlineRe.toString())
var newFromExplicitRe=new RegExp(explicitRe.source)
var evalFromExplicitRe=eval(explicitRe.toString())

shouldBe("inlineRe.source", "newFromInlineRe.source")
shouldBe("inlineRe.source", "evalFromInlineRe.source")
shouldBe("inlineRe.source", "evalInlineRe.source")
shouldBe("inlineRe.source", "newFromEvalInlineRe.source")
shouldBe("inlineRe.source", "evalFromEvalInlineRe.source")
shouldBe("inlineRe.source", "explicitRe.source")
shouldBe("inlineRe.source", "newFromExplicitRe.source")
shouldBe("inlineRe.source", "evalFromExplicitRe.source")

shouldBe("inlineRe.toString()", "newFromInlineRe.toString()")
shouldBe("inlineRe.toString()", "evalFromInlineRe.toString()")
shouldBe("inlineRe.toString()", "evalInlineRe.toString()")
shouldBe("inlineRe.toString()", "newFromEvalInlineRe.toString()")
shouldBe("inlineRe.toString()", "evalFromEvalInlineRe.toString()")
shouldBe("inlineRe.toString()", "explicitRe.toString()")
shouldBe("inlineRe.toString()", "newFromExplicitRe.toString()")
shouldBe("inlineRe.toString()", "evalFromExplicitRe.toString()")

shouldBe("inlineRe.exec(sample)[0]", "'bm\u2820p'")
shouldBe("evalInlineRe.exec(sample)[0]", "'bm\u2820p'")
shouldBe("explicitRe.exec(sample)[0]", "'bm\u2820p'")


// Test handling of \u007c "|"
var bsample="sample bm\u007cp cm\\u007cp";

var binlineRe=/.m\u007cp/
var bevalInlineRe=eval("/.m\\u007cp/")
var bexplicitRe=new RegExp(".m\\u007cp")
var bnewFromInlineRe=new RegExp(binlineRe.source)
var bevalFromInlineRe=eval(binlineRe.toString())
var bnewFromEvalInlineRe=new RegExp(bevalInlineRe.source)
var bevalFromEvalInlineRe=eval(bevalInlineRe.toString())
var bnewFromExplicitRe=new RegExp(bexplicitRe.source)
var bevalFromExplicitRe=eval(bexplicitRe.toString())

shouldBe("binlineRe.source", "bnewFromInlineRe.source")
shouldBe("binlineRe.source", "bevalFromInlineRe.source")
shouldBe("binlineRe.source", "bevalInlineRe.source")
shouldBe("binlineRe.source", "bnewFromEvalInlineRe.source")
shouldBe("binlineRe.source", "bevalFromEvalInlineRe.source")
shouldBe("binlineRe.source", "bexplicitRe.source")
shouldBe("binlineRe.source", "bnewFromExplicitRe.source")
shouldBe("binlineRe.source", "bevalFromExplicitRe.source")

shouldBe("binlineRe.toString()", "bnewFromInlineRe.toString()")
shouldBe("binlineRe.toString()", "bevalFromInlineRe.toString()")
shouldBe("binlineRe.toString()", "bevalInlineRe.toString()")
shouldBe("binlineRe.toString()", "bnewFromEvalInlineRe.toString()")
shouldBe("binlineRe.toString()", "bevalFromEvalInlineRe.toString()")
shouldBe("binlineRe.toString()", "bexplicitRe.toString()")
shouldBe("binlineRe.toString()", "bnewFromExplicitRe.toString()")
shouldBe("binlineRe.toString()", "bevalFromExplicitRe.toString()")

shouldBe("binlineRe.exec(bsample)[0]", "'bm|p'")
shouldBe("bevalInlineRe.exec(bsample)[0]", "'bm|p'")
shouldBe("bexplicitRe.exec(bsample)[0]", "'bm|p'")
