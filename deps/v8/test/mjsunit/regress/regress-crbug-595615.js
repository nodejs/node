// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

function f(o) {
  return o.x();
}
try { f({ x: 1 }); } catch(e) {}
try { f({ x: 1 }); } catch(e) {}
%OptimizeFunctionOnNextCall(f);
try { f({ x: 1 }); } catch(e) {}
