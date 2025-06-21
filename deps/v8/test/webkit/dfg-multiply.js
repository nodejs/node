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
"This tests that the DFG can multiply numbers correctly."
);

function doMultiplyConstant2(a) {
    return a * 2;
}

function doMultiplyConstant3(a) {
    return a * 3;
}

function doMultiplyConstant4(a) {
    return a * 4;
}

// Get it to compile.
for (var i = 0; i < 100; ++i) {
    shouldBe("doMultiplyConstant2(1)", "2");
    shouldBe("doMultiplyConstant2(2)", "4");
    shouldBe("doMultiplyConstant2(4)", "8");
    shouldBe("doMultiplyConstant3(1)", "3");
    shouldBe("doMultiplyConstant3(2)", "6");
    shouldBe("doMultiplyConstant3(4)", "12");
    shouldBe("doMultiplyConstant4(1)", "4");
    shouldBe("doMultiplyConstant4(2)", "8");
    shouldBe("doMultiplyConstant4(4)", "16");
}

// Now do evil.
for (var i = 0; i < 10; ++i) {
    shouldBe("doMultiplyConstant2(1073741824)", "2147483648");
    shouldBe("doMultiplyConstant2(2147483648)", "4294967296");
    shouldBe("doMultiplyConstant3(1073741824)", "3221225472");
    shouldBe("doMultiplyConstant3(2147483648)", "6442450944");
    shouldBe("doMultiplyConstant4(1073741824)", "4294967296");
    shouldBe("doMultiplyConstant4(2147483648)", "8589934592");
    shouldBe("doMultiplyConstant2(-1073741824)", "-2147483648");
    shouldBe("doMultiplyConstant2(-2147483648)", "-4294967296");
    shouldBe("doMultiplyConstant3(-1073741824)", "-3221225472");
    shouldBe("doMultiplyConstant3(-2147483648)", "-6442450944");
    shouldBe("doMultiplyConstant4(-1073741824)", "-4294967296");
    shouldBe("doMultiplyConstant4(-2147483648)", "-8589934592");
}
