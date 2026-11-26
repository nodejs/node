// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code --allow-natives-syntax

const padding = '/*' + ' '.repeat(65800) + '*/';

function body() {
  return `{
    if (!x) return;
    return (function child(which) {
      if (which) {
        let v;
        try { v = x; } catch (e) { return () => e; }
      }
      if (which) {
        const a = 1;
        return () => 11 + a - a;
      } else {
        const b = 2;
        return () => 22 + b - b;
      }
    });
  }`;
}

function testSync(head, firstArg, reparseArg) {
  const parent = (0, eval)(`(${head} => ${body()})`);
  const child = parent(firstArg);
  const first = child(true);
  %PrepareFunctionForOptimization(first);
  assertEquals(11, first());

  gc();

  parent(reparseArg);
  const second = child(false);
  %OptimizeFunctionOnNextCall(first);
  assertEquals(11, first());
  assertEquals(22, second());
}

function testAsync(head, firstArg, reparseArg) {
  const parent = (0, eval)(`(${head} => ${body()})`);
  let child;
  parent(firstArg).then(fn => { child = fn; });
  %PerformMicrotaskCheckpoint();
  const first = child(true);
  %PrepareFunctionForOptimization(first);
  assertEquals(11, first());

  gc();

  parent(reparseArg);
  %PerformMicrotaskCheckpoint();
  const second = child(false);
  %OptimizeFunctionOnNextCall(first);
  assertEquals(11, first());
  assertEquals(22, second());
}

testSync(`(x = 5 ${padding})`, 1, 0);
testSync(`(x = (5) ${padding})`, 1, 0);
testSync(`(x = 5 ${padding}, y)`, 1, 0);
testSync(`(x ${padding}, y = 1)`, 1, 0);
testSync(`(x = 5 ${padding},)`, 1, 0);
testSync(`({x} = {x: 5} ${padding})`, {x: 1}, {x: 0});
testSync(`([x] = [5] ${padding})`, [1], [0]);
testSync(`(...[x] ${padding})`, 1, 0);

testAsync(`async (x = 5 ${padding})`, 1, 0);
testAsync(`async ({x} = {x: 5} ${padding},)`, {x: 1}, {x: 0});
testAsync(`async (...[x] ${padding})`, 1, 0);

// Also verify CollectSourcePositions (crbug.com/577758569) does not observe a
// bytecode mismatch between ScopeInfo and reparsed arrow parameters.
let collectStack = false;
{
  const pad = 'a'.repeat(65480);
  const cmt = '/*' + ' '.repeat(40) + '*/';
  const outer = (0, eval)(
      `((p = "${pad}", x = 1 ${cmt}) => (() => {` +
      ` if (collectStack) new Error().stack; return x; })())`);
  assertEquals(1, outer());
  gc();
  collectStack = true;
  assertEquals(1, outer());
}
