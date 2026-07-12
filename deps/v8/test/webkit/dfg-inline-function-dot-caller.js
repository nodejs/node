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
"Tests that DFG inlining does not brak function.arguments.caller."
);

var callCount = 0;

var resultArray = []

function throwError() {
   throw {};
}
var object = {
   nonInlineable : function nonInlineable() {
       if (0) return [arguments, function(){}];
       if (++callCount == 999999) {
           var f = nonInlineable;
           while (f) {
               resultArray.push(f.name);
               f=f.arguments.callee.caller;
           }
       }
   },
   inlineable : function inlineable() {
       this.nonInlineable();
   }
}
function makeInlinableCall(o) {
   for (var i = 0; i < 1000; i++)
       o.inlineable();
}

function g() {
    var j = 0;
    for (var i = 0; i < 1000; i++) {
        j++;
        makeInlinableCall(object);
    }
}
g();

shouldBe("resultArray.length", "4");
shouldBe("resultArray[3]", "\"g\"");
shouldBe("resultArray[2]", "\"makeInlinableCall\"");
shouldBe("resultArray[1]", "\"inlineable\"");
shouldBe("resultArray[0]", "\"nonInlineable\"");
