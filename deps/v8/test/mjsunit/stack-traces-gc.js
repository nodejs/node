// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --expose-gc --allow-natives-syntax

var fired = [];
for (var i = 0; i < 100; i++) fired[i] = false;

function getter_function(i) {
  return %MarkOneShotGetter( function() { fired[i] = true; } );
}

// Error objects that die young.
for (var i = 0; i < 100; i++) {
  var error = new Error();
  // Replace the getter to observe whether it has been fired,
  // and disguise it as original getter.
  var getter = getter_function(i);
  error.__defineGetter__("stack", getter);

  error = undefined;
}

gc();
for (var i = 0; i < 100; i++) {
  assertFalse(fired[i]);
}

// Error objects that are kept alive.
var array = [];
for (var i = 0; i < 100; i++) {
  var error = new Error();
  var getter = getter_function(i);
  // Replace the getter to observe whether it has been fired,
  // and disguise it as original getter.
  error.__defineGetter__("stack", getter);

  array.push(error);
  error = undefined;
}

gc();
// We don't expect all stack traces to be formatted after only one GC.
assertTrue(fired[0]);

for (var i = 0; i < 10; i++) gc();
for (var i = 0; i < 100; i++) assertTrue(fired[i]);

// Error objects with custom stack getter.
var custom_error = new Error();
var custom_getter_fired = false;
custom_error.__defineGetter__("stack",
                              function() { custom_getter_fired = true; });
gc();
assertFalse(custom_getter_fired);

// Check that formatting caused by GC is not somehow observable.
var error;

var obj = { foo: function foo() { throw new Error(); } };

try {
  obj.foo();
} catch (e) {
  delete obj.foo;
  Object.defineProperty(obj, 'foo', {
    get: function() { assertUnreachable(); }
  });
  error = e;
}

gc();

Object.defineProperty(Array.prototype, '0', {
  get: function() { assertUnreachable(); }
});

try {
  throw new Error();
} catch (e) {
  error = e;
}

gc();

String.prototype.indexOf = function() { assertUnreachable(); };
String.prototype.lastIndexOf = function() { assertUnreachable(); };
var obj = { method: function() { throw Error(); } };
try {
  obj.method();
} catch (e) {
  error = e;
}

gc();
