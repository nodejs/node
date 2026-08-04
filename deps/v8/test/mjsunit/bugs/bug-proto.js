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

var realmA = Realm.current();
var realmB = Realm.create();
assertEquals(0, realmA);
assertEquals(1, realmB);

// The global objects match the realms' this binding.
assertSame(this, Realm.global(realmA));
assertSame(Realm.eval(realmB, "this"), Realm.global(realmB));
assertFalse(this === Realm.global(realmB));

// The global object is not accessible cross-realm.
var x = 3;
Realm.shared = this;
assertThrows("Realm.eval(realmB, 'x')");
assertSame(undefined, Realm.eval(realmB, "this.x"));
assertSame(undefined, Realm.eval(realmB, "Realm.shared.x"));

Realm.eval(realmB, "Realm.global(0).y = 1");
assertThrows("y");
assertSame(undefined, this.y);

// Can get or set other objects' properties cross-realm.
var p = {a: 1};
var o = {__proto__: p, b: 2};
Realm.shared = o;
assertSame(1, Realm.eval(realmB, "Realm.shared.a"));
assertSame(2, Realm.eval(realmB, "Realm.shared.b"));

// Cannot get or set a prototype cross-realm.
assertSame(undefined, Realm.eval(realmB, "Realm.shared.__proto__"));

Realm.eval(realmB, "Realm.shared.__proto__ = {c: 3}");
assertSame(1, o.a);
assertSame(undefined, o.c);
