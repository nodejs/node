// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining

[].values().extra = 1;
Object.prototype.values = Array.prototype.values;

let g = (function* () {})();

function side_effect() {}
%NeverOptimizeFunction(side_effect);

function get_undef() { return undefined; }
%PrepareFunctionForOptimization(get_undef);
%DisableEagerInlining(get_undef);

let do_load = true;
function r(it) {
  if (do_load) {
    side_effect();
    it.a;
    return g.next();
  }
}
%PrepareFunctionForOptimization(r);
%DisableEagerInlining(r);

function helper(x) {
  x.p;
  return r(x.values());
}
%PrepareFunctionForOptimization(helper);

function foo() {
  return helper(get_undef());
}
%PrepareFunctionForOptimization(foo);

r({ a: 42 });
do_load = false;
for (let i = 0; i < 20; i++) helper({});
try { foo(); } catch {}

%OptimizeMaglevOnNextCall(foo);
try { foo(); } catch {}
