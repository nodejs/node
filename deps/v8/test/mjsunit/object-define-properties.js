// Copyright 2010 the V8 project authors. All rights reserved.
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

// Tests the Object.defineProperties method - ES 15.2.3.7
// Note that the internal DefineOwnProperty method is tested through
// object-define-property.js, this file only contains tests specific for
// Object.defineProperties. Also note that object-create.js contains
// a range of indirect tests on this method since Object.create uses
// Object.defineProperties as a step in setting up the object.

// Try defining with null as descriptor:
try {
  Object.defineProperties({}, null);
} catch(e) {
  assertTrue(/null to object/.test(e));
}

// Try defining with null as object
try {
  Object.defineProperties(null, {});
} catch(e) {
  assertTrue(/called on non-object/.test(e));
}


var desc = {foo: {value: 10}, bar: {get: function() {return 42; }}};
var obj = {};
// Check that we actually get the object back as returnvalue
var x = Object.defineProperties(obj, desc);

assertEquals(x.foo, 10);
assertEquals(x.bar, 42);


// Make sure that all property descriptors are calculated before any
// modifications are done.

var object = {};

assertThrows(function() {
    Object.defineProperties(object, {
      foo: { value: 1 },
      bar: { value: 2, get: function() { return 3; } }
    });
  }, TypeError);

assertEquals(undefined, object.foo);
assertEquals(undefined, object.bar);
