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
"This tests that inlining preserves basic function.arguments functionality when said functionality is used from inside and outside getters and from inlined code, all at once."
);

function foo(o,b,c) {
    return [foo.arguments, bar.arguments].concat(o.f);
}

function fuzz(a, b) {
    return [foo.arguments, bar.arguments, getter.arguments, fuzz.arguments];
}

function getter() {
    return [foo.arguments, bar.arguments, getter.arguments].concat(fuzz(42, 56));
}

o = {}
o.__defineGetter__("f", getter);

function bar(o,b,c) {
    return [bar.arguments].concat(foo(o,b,c));
}

function argsToStr(args) {
    if (args.length === void 0 || args.charAt !== void 0)
        return "" + args
    var str = "[" + args + ": ";
    for (var i = 0; i < args.length; ++i) {
        if (i)
            str += ", ";
        str += argsToStr(args[i]);
    }
    return str + "]";
}

for (var __i = 0; __i < 200; ++__i) {
    var text1 = "[[object Arguments]: [object Object], b" + __i + ", c" + __i + "]";
    var text2 = "[[object Arguments]: ]";
    var text3 = "[[object Arguments]: 42, 56]";
    shouldBe("argsToStr(bar(o, \"b\" + __i, \"c\" + __i))", "\"[[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments],[object Arguments]: " + text1 + ", " + text1 + ", " + text1 + ", " + text1 + ", " + text1 + ", " + text2 + ", " + text1 + ", " + text1 + ", " + text2 + ", " + text3 + "]\"");
}
