// Copyright 2008 the V8 project authors. All rights reserved.
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

function load(o) {
  return o.x;
};

function store(o) {
  o.y = 42;
};

function call(o) {
  return o.f();
};

// Create a slow-case object (with hashed properties).
var o = { x: 42, f: function() { }, z: 100 };
delete o.z;

// Initialize IC stubs.
load(o);
store(o);
call(o);


// Create a new slow-case object (with hashed properties) and add
// setter and getter properties to the object.
var o = { z: 100 };
delete o.z;
o.__defineGetter__("x", function() { return 100; });
o.__defineSetter__("y", function(value) { this.y_mirror = value; });
o.__defineGetter__("f", function() { return function() { return 300; }});

// Perform the load checks.
assertEquals(100, o.x, "normal load");
assertEquals(100, load(o), "ic load");

// Perform the store checks.
o.y = 200;
assertEquals(200, o.y_mirror, "normal store");
store(o);
assertEquals(42, o.y_mirror, "ic store");

// Perform the call checks.
assertEquals(300, o.f(), "normal call");
assertEquals(300, call(o), "ic call");
