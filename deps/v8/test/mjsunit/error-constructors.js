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

var e = new Error();
assertFalse(e.hasOwnProperty('message'));
Error.prototype.toString = Object.prototype.toString;
assertEquals("[object Error]", Error.prototype.toString());
assertEquals(Object.prototype, Error.prototype.__proto__);

// Check that error construction does not call setters for the
// properties on error objects in prototypes.
function fail() { assertTrue(false); };
ReferenceError.prototype.__defineSetter__('stack', fail);
ReferenceError.prototype.__defineSetter__('message', fail);
ReferenceError.prototype.__defineSetter__('type', fail);
ReferenceError.prototype.__defineSetter__('arguments', fail);
var e0 = new ReferenceError();
var e1 = new ReferenceError('123');
assertTrue(e1.hasOwnProperty('message'));
assertTrue(e0.hasOwnProperty('stack'));
assertTrue(e1.hasOwnProperty('stack'));
assertTrue(e0.hasOwnProperty('type'));
assertTrue(e1.hasOwnProperty('type'));
assertTrue(e0.hasOwnProperty('arguments'));
assertTrue(e1.hasOwnProperty('arguments'));

// Check that the name property on error prototypes is read-only and
// dont-delete. This is not specified, but allowing overwriting the
// name property with a getter can leaks error objects from different
// script tags in the same context in a browser setting. We therefore
// disallow changes to the name property on error objects.
assertEquals("ReferenceError", ReferenceError.prototype.name);
delete ReferenceError.prototype.name;
assertEquals("ReferenceError", ReferenceError.prototype.name);
ReferenceError.prototype.name = "not a reference error";
assertEquals("ReferenceError", ReferenceError.prototype.name);

