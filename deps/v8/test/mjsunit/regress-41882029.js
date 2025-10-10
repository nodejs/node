// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

%RuntimeEvaluateREPL('let a = 42;');
%RuntimeEvaluateREPL('function read() { ++a; return a; }');
read();
read();
