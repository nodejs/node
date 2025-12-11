// Copyright 2011 the V8 project authors. All rights reserved.
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

// Check that message and name are not enumerable on Error objects.
var desc = Object.getOwnPropertyDescriptor(Error.prototype, 'name');
assertFalse(desc['enumerable']);
desc = Object.getOwnPropertyDescriptor(Error.prototype, 'message');
assertFalse(desc['enumerable']);

var e = new Error("foobar");
desc = Object.getOwnPropertyDescriptor(e, 'message');
assertFalse(desc['enumerable']);
desc = Object.getOwnPropertyDescriptor(e, 'stack');
assertFalse(desc['enumerable']);

var e = new Error();
assertFalse(e.hasOwnProperty('message'));

// name is not tested above, but in addition we should have no enumerable
// properties, so we simply assert that.
for (var v in e) {
  assertUnreachable();
}

// Check that error construction does not call setters for the
// properties on error objects in prototypes.
function fail() { assertUnreachable(); };
ReferenceError.prototype.__defineSetter__('name', fail);
ReferenceError.prototype.__defineSetter__('message', fail);
ReferenceError.prototype.__defineSetter__('stack', fail);

var e = new ReferenceError();
assertTrue(e.hasOwnProperty('stack'));

var e = new ReferenceError('123');
assertTrue(e.hasOwnProperty('message'));
assertTrue(e.hasOwnProperty('stack'));

try {
  eval("var error = reference");
} catch (error) {
  e = error;
}

assertTrue(e.hasOwnProperty('stack'));

// Check that intercepting property access from toString is prevented for
// compiler errors. This is not specified, but allowing interception through a
// getter can leak error objects from different script tags in the same context
// in a browser setting. Use Realm.eval as a proxy for loading scripts. We
// ignore the exception thrown from it since that would not be catchable from
// user-land code.
var errors = [SyntaxError, ReferenceError, TypeError, RangeError, URIError];
var error_triggers = ["syntax error",
                      "var error = reference",
                      "undefined()",
                      "String.fromCodePoint(0xFFFFFF)",
                      "decodeURI('%F')"];
for (var i in errors) {
  // Monkey-patch prototype.
  for (var prop of ["name", "message", "stack"]) {
    errors[i].prototype.__defineGetter__(prop, fail);
  }
  // String conversion should not invoke monkey-patched getters on prototype.
  assertThrows(()=>Realm.eval(0, error_triggers[i]));
}

// Monkey-patching non-internal errors should still be observable.
function MyError() {}
MyError.prototype = new Error;
var errors = [Error, RangeError, EvalError, URIError,
              SyntaxError, ReferenceError, TypeError, MyError];
for (var i in errors) {
  errors[i].prototype.__defineGetter__("name", function() { return "my"; });
  errors[i].prototype.__defineGetter__("message", function() { return "moo"; });
  var e = new errors[i];
  assertEquals("my: moo", e.toString());
}


Error.prototype.toString = Object.prototype.toString;
assertEquals("[object Object]", Error.prototype.toString());
assertEquals(Object.prototype, Error.prototype.__proto__);
var e = new Error("foo");
assertEquals("[object Error]", e.toString());
