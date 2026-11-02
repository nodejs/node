// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Resources: test/mjsunit/harmony/modules-skip-1.mjs

const other_realm = Realm.createAllowCrossRealmAccess();

// Populate the eval compilation cache from a script with a different origin
// ("(d8)" via Realm.eval) in both another Realm and the current Realm.
const eval_code = "import('../harmony/modules-skip-1.mjs')";
const func_code = "return import('../harmony/modules-skip-1.mjs')";

Realm.eval(other_realm, `(0, eval)(${JSON.stringify(eval_code)}).catch(() => {})`);
Realm.eval(other_realm, `new Function(${JSON.stringify(func_code)})().catch(() => {})`);
Realm.eval(Realm.current(), `(0, eval)(${JSON.stringify(eval_code)}).catch(() => {})`);
Realm.eval(Realm.current(), `new Function(${JSON.stringify(func_code)})().catch(() => {})`);

// Calling indirect eval and new Function from this script must not reuse the
// cached Script from Realm.eval, and must resolve the relative dynamic import
// against this script's path (test/mjsunit/regress/regress-561799397.mjs).
const ns1 = await (0, eval)(eval_code);
assertEquals(42, ns1.life());

const ns2 = await (new Function(func_code))();
assertEquals(42, ns2.life());

// Verify that Error stack frame eval origins also reflect the calling script,
// including when callers are inlined.
Realm.eval(other_realm, `(0, eval)("new Error()")`);
Error.prepareStackTrace = (err, frames) => frames[0].getEvalOrigin();
const origin = (0, eval)("new Error()").stack;
Error.prepareStackTrace = undefined;
assertTrue(origin.includes("regress-561799397.mjs"), origin);

function inlinedEvalA() {
  return (0, eval)('new Error().stack');
}
function inlinedEvalB() {
  return (0, eval)('new Error().stack');
}
function outerWrapper(which) {
  return which ? inlinedEvalA() : inlinedEvalB();
}

%PrepareFunctionForOptimization(inlinedEvalA);
%PrepareFunctionForOptimization(inlinedEvalB);
%PrepareFunctionForOptimization(outerWrapper);
assertMatches(/eval at inlinedEvalA/, outerWrapper(true));
assertMatches(/eval at inlinedEvalB/, outerWrapper(false));
%OptimizeFunctionOnNextCall(outerWrapper);
assertMatches(/eval at inlinedEvalA/, outerWrapper(true));
assertMatches(/eval at inlinedEvalB/, outerWrapper(false));

