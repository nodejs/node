// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class Test extends Number {}
Test.prototype[Symbol.toStringTag] = "Test";

function toString(o) {
  %ToFastProperties(o.__proto__);
  return Object.prototype.toString.call(o);
}

assertEquals("[object Test]", toString(new Test), "Try #1");
assertEquals("[object Test]", toString(new Test), "Try #2");
