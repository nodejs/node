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
"Tests that passing the global object to an array access that will arrayify to NonArrayWithArrayStorage doesn't break things."
);

function foo(array) {
    var result = 0;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}

function bar(array) {
    array[1] = 42;
}

var array = {};
array.length = 3;
array[0] = 1;
array[1] = 2;
array[2] = 3;
for (var i = 0; i < 200; ++i) {
    shouldBe("foo(array)", "6");

    var otherArray = {};
    bar(otherArray);
    shouldBe("otherArray[1]", "42");
}

for (var i = 0; i < 1000; ++i) {
    // Do strange things to ensure that the get_by_id on length goes polymorphic.
    var array = {};
    if (i % 2)
        array.x = 42;
    array.length = 3;
    array[0] = 1;
    array[2] = 3;
    array.__defineGetter__(1, function() { return 6; });

    shouldBe("foo(array)", "10");

    var otherArray = {};
    otherArray.__defineSetter__(0, function(value) { throw "error"; });
    bar(otherArray);
    shouldBe("otherArray[1]", "42");
}

var w = this;
w[0] = 1;
w.length = 1;
var thingy = false;
w.__defineSetter__(1, function(value) { thingy = value; });
shouldBe("foo(w)", "1");
shouldBe("thingy", "false");

// At this point we check to make sure that bar doesn't end up either creating array storage for
// the window proxy, or equally badly, storing to the already created array storage on the proxy
// (since foo() may have made the mistake of creating array storage). That's why we do the setter
// thingy, to detect that for index 1 we fall through the proxy to the real window object.
bar(w);

shouldBe("thingy", "42");
shouldBe("foo(w)", "1");
w.length = 2;
shouldBe("foo(w)", "0/0");
