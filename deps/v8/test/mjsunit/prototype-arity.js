// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const types = [
  [Object, "{}"],
  [String, "\"abc\""],
  [RegExp, "/abc/"],
  [WeakMap, "(new WeakMap())"],
  [WeakSet, "(new WeakSet())"],
  [Map, "(new Map())"],
  [Set, "(new Set())"],
  [Function, "(function f() {return 1})"],
  [Array, "[1,2,3, {}]"],
  [Boolean, "(new Boolean())"],
  [Symbol, "(new Symbol())"],
  [BigInt, "(new BigInt(42))"],
  [Math, "Math"],
  [Date, "(new Date())"],
  [Promise, "(new Promise())"],
  [Reflect, "Reflect"],
  [Proxy, "(new Proxy({}, {}))"],
];

if (typeof Intl == "object") {
  types.push([Intl, "Intl"]);
  types.push([Intl.Collator, "Intl.Collator"]);
  types.push([Intl.ListFormat, "Intl.ListFormat"]);
  types.push([Intl.NumberFormat, "Intl.NumberFormat"]);
  types.push([Intl.PluralRules, "Intl.PluralRules"]);
  types.push([Intl.RelativeTimeFormat, "Intl.RelativeTimeFormat"]);
}

const callTemplate = () => {
  function f() {
    return constr_exp.propCall(args)
  }
  %PrepareFunctionForOptimization(f);
  try { f(); } catch (e) {}
  try { f(); } catch (e) {}
  %OptimizeFunctionOnNextCall(f);
  try { f(); } catch (e) {}
}

const mkCall = (constr_exp, propCall) => {
  const arrowFunction = callTemplate.toString().replace("constr_exp", constr_exp).replace("propCall", propCall).replace("args", "");
  return `(${arrowFunction})();`;
}

for ([type, constr_exp, blacklist] of types) {
  const proto = type.prototype || type;
  for (const f of Object.getOwnPropertyNames(proto)) {
    const d = Object.getOwnPropertyDescriptor(proto, f);
    if (d.get || d.set || (typeof proto[f]) != "function") continue;
    const source = mkCall(constr_exp, f);
    try {
      eval(source);
    } catch (err) {
      // Exceptions are OK.
      console.log(`EXN ${err} for ${type.toString()} ${f}`)
      console.log(source);
      continue;
    }
  }
}
