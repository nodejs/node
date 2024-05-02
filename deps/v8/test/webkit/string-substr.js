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
"This test checks the boundary cases of substr()."
);

shouldBe("'bar'.substr(0)", "'bar'");
shouldBe("'bar'.substr(3)", "''");
shouldBe("'bar'.substr(4)", "''");
shouldBe("'bar'.substr(-1)", "'r'");
shouldBe("'bar'.substr(-3)", "'bar'");
shouldBe("'bar'.substr(-4)", "'bar'");

shouldBe("'bar'.substr(0, 0)", "''");
shouldBe("'bar'.substr(0, 1)", "'b'");
shouldBe("'bar'.substr(0, 3)", "'bar'");
shouldBe("'bar'.substr(0, 4)", "'bar'");

shouldBe("'bar'.substr(1, 0)", "''");
shouldBe("'bar'.substr(1, 1)", "'a'");
shouldBe("'bar'.substr(1, 2)", "'ar'");
shouldBe("'bar'.substr(1, 3)", "'ar'");

shouldBe("'bar'.substr(3, 0)", "''");
shouldBe("'bar'.substr(3, 1)", "''");
shouldBe("'bar'.substr(3, 3)", "''");

shouldBe("'bar'.substr(4, 0)", "''");
shouldBe("'bar'.substr(4, 1)", "''");
shouldBe("'bar'.substr(4, 3)", "''");

shouldBe("'bar'.substr(-1, 0)", "''");
shouldBe("'bar'.substr(-1, 1)", "'r'");

shouldBe("'bar'.substr(-3, 1)", "'b'");
shouldBe("'bar'.substr(-3, 3)", "'bar'");
shouldBe("'bar'.substr(-3, 4)", "'bar'");

shouldBe("'bar'.substr(-4)", "'bar'");
shouldBe("'bar'.substr(-4, 0)", "''");
shouldBe("'bar'.substr(-4, 1)", "'b'");
shouldBe("'bar'.substr(-4, 3)", "'bar'");
shouldBe("'bar'.substr(-4, 4)", "'bar'");

shouldBe("'GMAIL_IMP=bf-i%2Fd-0-0%2Ftl-v'.substr(10)", "'bf-i%2Fd-0-0%2Ftl-v'");
