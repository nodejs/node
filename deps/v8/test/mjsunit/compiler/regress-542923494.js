// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --concurrent-recompilation
// Flags: --concurrent-recompilation-delay=50

// A function whose .prototype is a primitive keeps its initial map in a Tuple2
// instead of directly in prototype_or_initial_map. Installing the initial map
// mutates that already-published tuple, while a background compilation can
// reach the same map by reading the function out of a context slot and
// following the tuple (JSFunctionData::Cache).

(function() {
  function Goo() {}
  Goo.prototype = 42;

  function opt() { return Goo; }

  %PrepareFunctionForOptimization(opt);
  opt();
  %OptimizeFunctionOnNextCall(opt, "concurrent");
  // Queues the background job. The recompilation delay keeps it from running
  // until the initial map below has been installed.
  opt();

  new Goo();

  %WaitForBackgroundOptimization();
  assertSame(Goo, opt());
})();
