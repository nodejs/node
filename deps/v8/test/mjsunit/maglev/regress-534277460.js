// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-lazy-feedback-allocation

function trigger() {
  let gen = (function*() {})();
  for (let minus_one = -1; true;) {
    %_GeneratorClose(gen);
    [].forEach(minus_one);
  }
}

%PrepareFunctionForOptimization(trigger);
try { trigger(); } catch(e) {}
try { trigger(); } catch(e) {}
%OptimizeMaglevOnNextCall(trigger);
try { trigger(); } catch(e) {}
