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

// Flags: --allow-natives-syntax --noconcurrent-recompilation

if (%IsConcurrentRecompilationSupported()) {
  print("Concurrent recompilation is turned on after all. Skipping this test.");
  quit();
}

/**
 * This class shows how to use %GetOptimizationCount() and
 * %GetOptimizationStatus() to infer information about opts and deopts.
 * Might be nice to put this into mjsunit.js, but that doesn't depend on
 * the --allow-natives-syntax flag so far.
 */
function OptTracker() {
  this.opt_counts_ = {};
}

/**
 * The possible optimization states of a function. Must be in sync with the
 * return values of Runtime_GetOptimizationStatus() in runtime.cc!
 * @enum {int}
 */
OptTracker.OptimizationState = {
    YES: 1,
    NO: 2,
    ALWAYS: 3,
    NEVER: 4
};

/**
 * Always call this at the beginning of your test, once for each function
 * that you later want to track de/optimizations for. It is necessary because
 * tests are sometimes executed several times in a row, and you want to
 * disregard counts from previous runs.
 */
OptTracker.prototype.CheckpointOptCount = function(func) {
  this.opt_counts_[func] = %GetOptimizationCount(func);
};

OptTracker.prototype.AssertOptCount = function(func, optcount) {
  if (this.DisableAsserts_(func)) {
    return;
  }
  assertEquals(optcount, this.GetOptCount_(func));
};

OptTracker.prototype.AssertDeoptCount = function(func, deopt_count) {
  if (this.DisableAsserts_(func)) {
    return;
  }
  assertEquals(deopt_count, this.GetDeoptCount_(func));
};

OptTracker.prototype.AssertDeoptHappened = function(func, expect_deopt) {
  if (this.DisableAsserts_(func)) {
    return;
  }
  if (expect_deopt) {
    assertTrue(this.GetDeoptCount_(func) > 0);
  } else {
    assertEquals(0, this.GetDeoptCount_(func));
  }
}

OptTracker.prototype.AssertIsOptimized = function(func, expect_optimized) {
  if (this.DisableAsserts_(func)) {
    return;
  }
  var raw_optimized = %GetOptimizationStatus(func);
  if (expect_optimized) {
    assertEquals(OptTracker.OptimizationState.YES, raw_optimized);
  } else {
    assertEquals(OptTracker.OptimizationState.NO, raw_optimized);
  }
}

/**
 * @private
 */
OptTracker.prototype.GetOptCount_ = function(func) {
  var raw_count = %GetOptimizationCount(func);
  if (func in this.opt_counts_) {
    var checkpointed_count = this.opt_counts_[func];
    return raw_count - checkpointed_count;
  }
  return raw_count;
}

/**
 * @private
 */
OptTracker.prototype.GetDeoptCount_ = function(func) {
  var count = this.GetOptCount_(func);
  if (%GetOptimizationStatus(func) == OptTracker.OptimizationState.YES) {
    count -= 1;
  }
  return count;
}

/**
 * @private
 */
OptTracker.prototype.DisableAsserts_ = function(func) {
  switch(%GetOptimizationStatus(func)) {
    case OptTracker.OptimizationState.YES:
    case OptTracker.OptimizationState.NO:
      return false;
    case OptTracker.OptimizationState.ALWAYS:
    case OptTracker.OptimizationState.NEVER:
      return true;
  }
  return false;
}
// (End of class OptTracker.)

// Example function used by the test below.
function f(a) {
  return a+1;
}

var tracker = new OptTracker();
tracker.CheckpointOptCount(f);

tracker.AssertOptCount(f, 0);
tracker.AssertIsOptimized(f, false);
tracker.AssertDeoptHappened(f, false);
tracker.AssertDeoptCount(f, 0);

f(1);

%OptimizeFunctionOnNextCall(f);
f(1);

tracker.AssertOptCount(f, 1);
tracker.AssertIsOptimized(f, true);
tracker.AssertDeoptHappened(f, false);
tracker.AssertDeoptCount(f, 0);

%DeoptimizeFunction(f);

tracker.AssertOptCount(f, 1);
tracker.AssertIsOptimized(f, false);
tracker.AssertDeoptHappened(f, true);
tracker.AssertDeoptCount(f, 1);

// Let's trigger optimization for another type.
for (var i = 0; i < 5; i++) f("a");

%OptimizeFunctionOnNextCall(f);
f("b");

tracker.AssertOptCount(f, 2);
tracker.AssertIsOptimized(f, true);
tracker.AssertDeoptHappened(f, true);
tracker.AssertDeoptCount(f, 1);
