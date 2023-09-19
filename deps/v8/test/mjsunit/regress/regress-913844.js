// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --disable-in-process-stack-traces --perf-prof-unwinding-info --turbo-loop-rotation
for (var x = 0; x < 1000000; x++)
;
