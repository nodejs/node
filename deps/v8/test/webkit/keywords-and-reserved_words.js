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
"This test verifies that keywords and reserved words match those specified in ES5 section 7.6."
);

function isKeyword(x)
{
    try {
        eval("var "+x+";");
    } catch(e) {
        return true;
    }

    return false;
}

function isStrictKeyword(x)
{
    try {
        eval("'use strict'; var "+x+";");
    } catch(e) {
        return true;
    }

    return false;
}

function classifyIdentifier(x)
{
    if (isKeyword(x)) {
        // All non-strict keywords are also keywords in strict code.
        if (!isStrictKeyword(x))
            return "ERROR";
        return "keyword";
    }

    // Check for strict mode future reserved words.
    if (isStrictKeyword(x))
        return "strict";

    return "identifier";
}

// Not keywords - these are all just identifiers.
shouldBe('classifyIdentifier("x")', '"identifier"');
shouldBe('classifyIdentifier("id")', '"identifier"');
shouldBe('classifyIdentifier("identifier")', '"identifier"');
shouldBe('classifyIdentifier("keyword")', '"identifier"');
shouldBe('classifyIdentifier("strict")', '"identifier"');
shouldBe('classifyIdentifier("use")', '"identifier"');
// These are identifiers that we used to treat as keywords!
shouldBe('classifyIdentifier("abstract")', '"identifier"');
shouldBe('classifyIdentifier("boolean")', '"identifier"');
shouldBe('classifyIdentifier("byte")', '"identifier"');
shouldBe('classifyIdentifier("char")', '"identifier"');
shouldBe('classifyIdentifier("double")', '"identifier"');
shouldBe('classifyIdentifier("final")', '"identifier"');
shouldBe('classifyIdentifier("float")', '"identifier"');
shouldBe('classifyIdentifier("goto")', '"identifier"');
shouldBe('classifyIdentifier("int")', '"identifier"');
shouldBe('classifyIdentifier("long")', '"identifier"');
shouldBe('classifyIdentifier("native")', '"identifier"');
shouldBe('classifyIdentifier("short")', '"identifier"');
shouldBe('classifyIdentifier("synchronized")', '"identifier"');
shouldBe('classifyIdentifier("throws")', '"identifier"');
shouldBe('classifyIdentifier("transient")', '"identifier"');
shouldBe('classifyIdentifier("volatile")', '"identifier"');

// Keywords.
shouldBe('classifyIdentifier("break")', '"keyword"');
shouldBe('classifyIdentifier("case")', '"keyword"');
shouldBe('classifyIdentifier("catch")', '"keyword"');
shouldBe('classifyIdentifier("continue")', '"keyword"');
shouldBe('classifyIdentifier("debugger")', '"keyword"');
shouldBe('classifyIdentifier("default")', '"keyword"');
shouldBe('classifyIdentifier("delete")', '"keyword"');
shouldBe('classifyIdentifier("do")', '"keyword"');
shouldBe('classifyIdentifier("else")', '"keyword"');
shouldBe('classifyIdentifier("finally")', '"keyword"');
shouldBe('classifyIdentifier("for")', '"keyword"');
shouldBe('classifyIdentifier("function")', '"keyword"');
shouldBe('classifyIdentifier("if")', '"keyword"');
shouldBe('classifyIdentifier("in")', '"keyword"');
shouldBe('classifyIdentifier("instanceof")', '"keyword"');
shouldBe('classifyIdentifier("new")', '"keyword"');
shouldBe('classifyIdentifier("return")', '"keyword"');
shouldBe('classifyIdentifier("switch")', '"keyword"');
shouldBe('classifyIdentifier("this")', '"keyword"');
shouldBe('classifyIdentifier("throw")', '"keyword"');
shouldBe('classifyIdentifier("try")', '"keyword"');
shouldBe('classifyIdentifier("typeof")', '"keyword"');
shouldBe('classifyIdentifier("var")', '"keyword"');
shouldBe('classifyIdentifier("void")', '"keyword"');
shouldBe('classifyIdentifier("while")', '"keyword"');
shouldBe('classifyIdentifier("with")', '"keyword"');
// Technically these are "Future Reserved Words"!
shouldBe('classifyIdentifier("class")', '"keyword"');
shouldBe('classifyIdentifier("const")', '"keyword"');
shouldBe('classifyIdentifier("enum")', '"keyword"');
shouldBe('classifyIdentifier("export")', '"keyword"');
shouldBe('classifyIdentifier("extends")', '"keyword"');
shouldBe('classifyIdentifier("import")', '"keyword"');
shouldBe('classifyIdentifier("super")', '"keyword"');

// Future Reserved Words, in strict mode only.
shouldBe('classifyIdentifier("implements")', '"strict"');
shouldBe('classifyIdentifier("interface")', '"strict"');
shouldBe('classifyIdentifier("let")', '"strict"');
shouldBe('classifyIdentifier("package")', '"strict"');
shouldBe('classifyIdentifier("private")', '"strict"');
shouldBe('classifyIdentifier("protected")', '"strict"');
shouldBe('classifyIdentifier("public")', '"strict"');
shouldBe('classifyIdentifier("static")', '"strict"');
shouldBe('classifyIdentifier("yield")', '"strict"');
