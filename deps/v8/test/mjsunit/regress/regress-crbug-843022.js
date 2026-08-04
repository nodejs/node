// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Produce an fast, but empty result.
const fast_regexp_result = /./g.exec("a");
fast_regexp_result.length = 0;
class RegExpWithFastResult extends RegExp {
  constructor() { super(".", "g"); this.number_of_runs = 0; }
  exec(str) { return (this.number_of_runs++ == 0) ? fast_regexp_result : null; }
}

// A slow empty result.
const slow_regexp_result = [];
class RegExpWithSlowResult extends RegExp {
  constructor() { super(".", "g"); this.number_of_runs = 0; }
  exec(str) { return (this.number_of_runs++ == 0) ? slow_regexp_result : null; }
}

assertEquals(["undefined"], "a".match(new RegExpWithFastResult()));
assertEquals(["undefined"], "a".match(new RegExpWithSlowResult()));
