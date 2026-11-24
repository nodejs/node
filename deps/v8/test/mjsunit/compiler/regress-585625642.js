// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// 1. JSLoadContextNoCell (depth = 0, 2, 3) and JSStoreContextNoCell (depth = 2)
//    inside resumable generator loops with a smaller inner BlockContext.
function makeGenerator() {
  {
    let outer_mut = 0;
    const outer_const = 100;
    () => [outer_mut, outer_const];
    {
      let mid_block = 0;
      () => mid_block;

      return function* gen(opt, flag, val, k) {
        const x0 = val, x1 = val, x2 = val, x3 = val, x4 = val,
              x5 = val, x6 = val, x7 = val, x8 = val, x9 = val;
        () => [x0, x1, x2, x3, x4, x5, x6, x7, x8, x9];
        for (let i = 0; i < 2; i++) {
          if (opt === 1) {
            const a = x9;
            const b = outer_const;
            outer_mut = a;
            return a + b;
          }
          do {
            if (opt === 2) return x9 + mid_block;
            if (!flag) {
              if (val === null) return 0.1 + (k === null);
              {
                const s = val;
                () => s;
                const resumed = yield val;
                if (val === undefined) return 0.1 + (k === null);
                if (resumed !== undefined) return s + outer_mut;
              }
            }
            if (opt === -1) continue;
            break;
          } while (true);
        }
        return x9;
      };
    }
  }
}

const gen = makeGenerator();
%PrepareFunctionForOptimization(gen);
for (let i = 0; i < 10; i++) {
  assertEquals({value: 142, done: true}, gen(1, false, 42, 1).next());
  assertEquals({value: 42, done: true}, gen(2, false, 42, 1).next());
  assertEquals({value: 1.1, done: true}, gen(0, false, null, null).next());

  let g1 = gen(0, false, undefined, null);
  assertEquals({value: undefined, done: false}, g1.next());
  assertEquals({value: 1.1, done: true}, g1.next());

  let g2 = gen(0, false, 7, 1);
  assertEquals({value: 7, done: false}, g2.next());
  assertEquals({value: 49, done: true}, g2.next('done'));

  let g3 = gen(0, true, 9, 1);
  assertEquals({value: 9, done: true}, g3.next());
}

%OptimizeFunctionOnNextCall(gen);

assertEquals({value: 142, done: true}, gen(1, false, 42, 1).next());
assertEquals({value: 42, done: true}, gen(2, false, 42, 1).next());
assertEquals({value: 1.1, done: true}, gen(0, false, null, null).next());

let g1 = gen(0, false, undefined, null);
assertEquals({value: undefined, done: false}, g1.next());
assertEquals({value: 1.1, done: true}, g1.next());

let g2 = gen(0, false, 7, 1);
assertEquals({value: 7, done: false}, g2.next());
assertEquals({value: 49, done: true}, g2.next('done'));

let g3 = gen(0, true, 9, 1);
assertEquals({value: 9, done: true}, g3.next());

let g = gen(0, false, 13, 1);
assertEquals({value: 13, done: false}, g.next());
assertEquals({value: 13, done: false}, g.next());
assertEquals({value: 13, done: true}, g.next());

// 2. JSHasContextExtension (depth = 0 and depth = 1 via
//    CheckContextExtensionsSlowPath when loop-header context is a Phi merging
//    FunctionContext and a smaller BlockContext).
function makeSloppyLookupGenerator() {
  {
    let target = 55;
    () => target;
    {
      let mid = 0;
      () => mid;
      return function* lookupGen(use_eval, opt, val) {
        if (use_eval) eval('var shadowed = 1;');
        for (let i = 0; i < 2; i++) {
          if (opt) return target;
          {
            const s = val;
            () => s;
            yield s;
          }
        }
        return target;
      };
    }
  }
}

const lookupGen = makeSloppyLookupGenerator();
%PrepareFunctionForOptimization(lookupGen);
for (let i = 0; i < 10; i++) {
  assertEquals({value: 55, done: true}, lookupGen(false, true, 3).next());
  let lg = lookupGen(false, false, 3);
  assertEquals({value: 3, done: false}, lg.next());
  assertEquals({value: 3, done: false}, lg.next());
  assertEquals({value: 55, done: true}, lg.next());
}

%OptimizeFunctionOnNextCall(lookupGen);

assertEquals({value: 55, done: true}, lookupGen(false, true, 3).next());
let lg = lookupGen(false, false, 8);
assertEquals({value: 8, done: false}, lg.next());
assertEquals({value: 8, done: false}, lg.next());
assertEquals({value: 55, done: true}, lg.next());
