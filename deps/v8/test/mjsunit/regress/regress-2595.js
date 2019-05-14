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

var p = { f: function () { return "p"; } };
var o = Object.create(p);
o.x = true;
delete o.x;  // slow case object

var u = { x: 0, f: function () { return "u"; } };  // object with some other map

function F(x) {
  return x.f();
}

// First make CALL IC in F go MEGAMORPHIC and ensure that we put the stub
// that calls p.f (guarded by a negative dictionary lookup on the receiver)
// into the stub cache
assertEquals("p", F(o));
assertEquals("p", F(o));
assertEquals("u", F(u));
assertEquals("p", F(o));
assertEquals("u", F(u));

// Optimize F. We will inline p.f into F guarded by map checked against
// receiver which does not work for slow case objects.
%OptimizeFunctionOnNextCall(F);
assertEquals("p", F(o));

// Add f to o. o's map will *not* change.
o.f = function () { return "o"; };
assertEquals("o", F(o));
