// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax

// The type feedback oracle had a bug when retrieving the map from an IC
// starting with a negative lookup.

// Create a holder in fast mode.
var holder = Object.create(null, {
  holderMethod: {value: function() {}}
});
assertTrue(%HasFastProperties(holder));

// Create a receiver into dictionary mode.
var receiver = Object.create(holder, {
  killMe: {value: 0, configurable: true},
});
delete receiver.killMe;
assertFalse(%HasFastProperties(receiver));

// The actual function to test, triggering the retrieval of the wrong map.
function callConstantFunctionOnPrototype(obj) {
  obj.holderMethod();
}

callConstantFunctionOnPrototype(receiver);
callConstantFunctionOnPrototype(receiver);
%OptimizeFunctionOnNextCall(callConstantFunctionOnPrototype);
callConstantFunctionOnPrototype(receiver);

// Make sure that the function is still optimized.
assertOptimized(callConstantFunctionOnPrototype);
