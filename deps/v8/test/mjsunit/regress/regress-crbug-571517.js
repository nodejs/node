// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Receiver() { this.receiver = "receiver"; }
function Proto() { this.proto = "proto"; }

function f(a) {
  return a.foo;
}

var rec = new Receiver();

// Formerly, this mutated rec.__proto__.__proto__, but
// the global object prototype chain is now immutable;
// not sure if this test now hits the original hazard case.
var proto = rec.__proto__;

// Initialize prototype chain dependent IC (nonexistent load).
assertEquals(undefined, f(rec));
assertEquals(undefined, f(rec));

// Add a new prototype to the end of the chain.
var p2 = new Proto();
p2.__proto__ = null;
proto.__proto__ = p2;

// Update the IC.
assertEquals(undefined, f(rec));

// Now modify the most recently added prototype by adding a property...
p2.foo = "bar";
assertEquals("bar", f(rec));

// ...and removing it again. Due to missing prototype user registrations,
// this fails to invalidate the IC.
delete p2.foo;
p2.secret = "GAME OVER";
assertEquals(undefined, f(rec));
