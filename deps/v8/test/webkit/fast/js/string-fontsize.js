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
'This is a test case for String.prototype.fontsize(size).'
);

// This test is based on http://mathias.html5.org/tests/javascript/string/.

// Check that the quotation mark is correctly escaped.
shouldBe("'_'.fontsize('\"')", '"<font size=\\"&quot;\\">_</font>"');

// Simple case.
shouldBe("'_'.fontsize('b')", '"<font size=\\"b\\">_</font>"');

// Does not escape special characters in `this` value.
shouldBe("'<'.fontsize('b')", '"<font size=\\"b\\"><</font>"');

// First argument gets ToString()ed.
shouldBe("'_'.fontsize(0x2A)", '"<font size=\\"42\\">_</font>"');

// Check that the quotation mark is correctly escaped.
shouldBe("'_'.fontsize('\"')", '"<font size=\\"&quot;\\">_</font>"');
shouldBe("'_'.fontsize('\" color=\"b')", '"<font size=\\"&quot; color=&quot;b\\">_</font>"');

// Generic use on Number object.
shouldBe("String.prototype.fontsize.call(0x2A, 0x2A)", '"<font size=\\"42\\">42</font>"');

// Generic use on non-coercible object `undefined`.
shouldThrow("String.prototype.fontsize.call(undefined)", '"TypeError: Type error"');

// Generic use on non-coercible object `null`.
shouldThrow("String.prototype.fontsize.call(null)", '"TypeError: Type error"');

// Check fontsize.length.
shouldBe("String.prototype.fontsize.length", "1");
