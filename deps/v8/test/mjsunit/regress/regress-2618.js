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

// Flags: --use-osr --allow-natives-syntax --turbofan
// Flags: --no-always-turbofan --no-maglev-osr

// Can't OSR with always-turbofan or in Lite mode.
if (isNeverOptimizeLiteMode()) {
  print("Warning: skipping test that requires optimization in Lite mode.");
  testRunner.quit(0);
}
assertFalse(isAlwaysOptimize());

function f(disable_asserts) {
  do {
    do {
      for (var i = 0; i < 10; i++) {
        %OptimizeOsr();
        %PrepareFunctionForOptimization(f);
      }
      // Note: this check can't be wrapped in a function, because
      // calling that function causes a deopt from lack of call
      // feedback.
      var opt_status = %GetOptimizationStatus(f);
      assertTrue(
        disable_asserts ||
        (opt_status & V8OptimizationStatus.kMaybeDeopted) !== 0 ||
        (opt_status & V8OptimizationStatus.kTopmostFrameIsTurboFanned) !== 0);
    } while (false);
  } while (false);
}

%PrepareFunctionForOptimization(f);
f(true);  // Gather feedback first.
f(false);

function g() {
  for (var i = 0; i < 1; i++) { }

  do {
    do {
      for (var i = 0; i < 1; i++) { }
    } while (false);
  } while (false);

  do {
    do {
      do {
        do {
          do {
            do {
              do {
                do {
                  for (var i = 0; i < 10; i++) {
                    %OptimizeOsr();
                    %PrepareFunctionForOptimization(g);
                  }
                  var opt_status = %GetOptimizationStatus(g);
                  assertTrue(
                    (opt_status & V8OptimizationStatus.kMaybeDeopted) !== 0 ||
                    (opt_status &
                        V8OptimizationStatus.kTopmostFrameIsTurboFanned) !== 0);
                } while (false);
              } while (false);
            } while (false);
          } while (false);
        } while (false);
      } while (false);
    } while (false);
  } while (false);
}

%PrepareFunctionForOptimization(g);
g();
