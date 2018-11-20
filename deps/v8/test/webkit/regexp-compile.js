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
'Test RegExp.compile method.'
);

re = new RegExp("a", "i");
shouldBe("re.toString()", "'/a/i'");

re.compile("a");
shouldBe("re.multiline", "false");
shouldBe("re.ignoreCase", "false");
shouldBe("re.global", "false");
shouldBe("re.test('A')", "false");
shouldBe("re.toString()", "'/a/'");

re.compile("b", "g");
shouldBe("re.toString()", "'/b/g'");

re.compile(new RegExp("c"));
shouldBe("re.toString()", "'/c/'");

re.compile(new RegExp("c", "i"));
shouldBe("re.ignoreCase", "true");
shouldBe("re.test('C')", "true");
shouldBe("re.toString()", "'/c/i'");

shouldThrow("re.compile(new RegExp('c'), 'i');");

// It's OK to supply a second argument, as long as the argument is "undefined".
re.compile(re, undefined);
shouldBe("re.toString()", "'/c/i'");

shouldThrow("re.compile(new RegExp('+'));");

re.compile(undefined);
shouldBe("re.toString()", "'/undefined/'");

re.compile(null);
shouldBe("re.toString()", "'/null/'");

re.compile();
shouldBe("re.toString()", "'/(?:)/'");

re.compile("z", undefined);
shouldBe("re.toString()", "'/z/'");

// Compiling should reset lastIndex.
re.lastIndex = 100;
re.compile(/a/g);
shouldBe("re.lastIndex", "0");
re.exec("aaa");
shouldBe("re.lastIndex", "1");
