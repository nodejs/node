// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestSubclassRegExpSplitVFlag() {
  let execCalls = [];
  class CustomRegExp extends RegExp {
    exec(str) {
      execCalls.push(this.lastIndex);
      if (this.lastIndex === 1) {
        return Object.assign([""], { index: 1 });
      }
      return null;
    }
  }

  // With 'u' flag, AdvanceStringIndex advances over the surrogate pair
  // (\uD842\uDFB7) from index 0 to 2, so exec is only called at lastIndex 0.
  const reU = new CustomRegExp("", "u");
  execCalls = [];
  assertEquals(["𠮷"], reU[Symbol.split]("𠮷"));
  assertEquals([0], execCalls);

  // With 'v' flag, AdvanceStringIndex should likewise advance over the
  // surrogate pair from index 0 to 2 per spec (unicodeMatching is true when
  // flags contains 'u' or 'v').
  const reV = new CustomRegExp("", "v");
  execCalls = [];
  assertEquals(["𠮷"], reV[Symbol.split]("𠮷"));
  assertEquals([0], execCalls);
})();

(function TestOverrideExecRegExpSplitVFlag() {
  let execCalls = [];
  const originalExec = RegExp.prototype.exec;
  RegExp.prototype.exec = function(str) {
    execCalls.push(this.lastIndex);
    if (this.lastIndex === 1) {
      return Object.assign([""], { index: 1 });
    }
    return null;
  };

  try {
    execCalls = [];
    assertEquals(["𠮷"], /(?:)/u[Symbol.split]("𠮷"));
    assertEquals([0], execCalls);

    execCalls = [];
    assertEquals(["𠮷"], /(?:)/v[Symbol.split]("𠮷"));
    assertEquals([0], execCalls);
  } finally {
    RegExp.prototype.exec = originalExec;
  }
})();
