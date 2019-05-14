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
"This tests that common subexpression elimination knows how to accurately model PutBuVal."
);

function doAccesses(a, b, i, j, y) {
    var x = a[i];
    b[j] = y;
    return a[i];
}

var array1 = [1, 2, 3, 4];
var array2 = [5, 6, 7, 8];

for (var i = 0; i < 1000; ++i) {
    // Simple test, pretty easy to pass.
    shouldBe("doAccesses(array1, array2, i % 4, (i + 1) % 4, i)", "" + ((i % 4) + 1));
    shouldBe("array2[" + ((i + 1) % 4) + "]", "" + i);

    // Undo.
    array2[((i + 1) % 4)] = (i % 4) + 5;

    // Now the evil test. This is constructed to minimize the likelihood that CSE will succeed through
    // cleverness alone.
    shouldBe("doAccesses(array1, array1, i % 4, 0, i)", "" + ((i % 4) == 0 ? i : (i % 4) + 1));

    // Undo.
    array1[0] = 1;
}
