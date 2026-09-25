// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Stringified(toString) {
  var valueOf = "-" + toString + "-";
  return {
    toString: function() { return toString; },
    valueOf: function() { return valueOf; }
  };
}

assertEquals("a.b.", "a.".concat(Stringified("b.")));
assertEquals("a.b.c.", "a.".concat(Stringified("b."), Stringified("c.")));
