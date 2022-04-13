// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --super-ic --sparkplug --no-always-sparkplug

function run(f, ...args) {
  try { f(...args); } catch (e) {}
  %CompileBaseline(f);
  return f(...args);
}

function construct(f, ...args) {
  try { new f(...args); } catch (e) {}
  %CompileBaseline(f);
  return new f(...args);
}

// Constants
assertEquals(run(()=>undefined), undefined);
assertEquals(run(()=>null), null);
assertEquals(run(()=>true), true);
assertEquals(run(()=>false), false);
assertEquals(run(()=>"bla"), "bla");
assertEquals(run(()=>42), 42);
assertEquals(run(()=>0), 0);

// Variables
assertEquals(run(()=>{let a = 42; return a}), 42);
assertEquals(run(()=>{let a = 42; let b = 32; return a}), 42);

// Arguments
assertEquals(run((a)=>a, 42), 42);
assertEquals(run((a,b)=>b, 1, 42), 42);
assertEquals(run((a,b,c)=>c, 1, 2, 42), 42);

// Property load
assertEquals(run((o)=>o.a, {a:42}), 42);
assertEquals(run((o, k)=>o[k], {a:42}, "a"), 42);

// Property store
assertEquals(run((o)=>{o.a=42; return o}, {}).a, 42);
assertEquals(run((o, k)=>{o[k]=42; return o}, {}, "a").a, 42);

// Global load/store
global_x = 45;
assertEquals(run(()=>global_x), 45);
run(()=>{ global_x = 49 })
assertEquals(global_x, 49);

// Context load
(function () {
  let x = 42;
  assertEquals(run(()=>{return x;}), 42);
})();
(function () {
  let x = 4;
  x = 42;
  assertEquals(run(()=>{return x;}), 42);
})();

// Context store
(function () {
  let x = 4;
  run(()=>{x = 42;});
  assertEquals(x, 42);
})();


// Super
// var o = {__proto__:{a:42}, m() { return super.a }};
// assertEquals(run(o.m), 42);

// Control flow
assertEquals(run((x)=>{ if(x) return 5; return 10;}), 10);
assertEquals(run(()=>{ var x = 0; for(var i = 1; i; i=0) x=10; return x;}), 10);
assertEquals(run(()=>{ var x = 0; for(var i = 0; i < 10; i+=1) x+=1; return x;}), 10);
assertEquals(run(()=>{ var x = 0; for(var i = 0; i < 10; ++i) x+=1; return x;}), 10);

// Typeof
function testTypeOf(o, t) {
  let types = ['number', 'string', 'symbol', 'boolean', 'bigint', 'undefined',
               'function', 'object'];
  assertEquals(t, eval('run(()=>typeof ' + o + ')'),
               `(()=>typeof ${o})() == ${t}`);
  assertTrue(eval('run(()=>typeof ' + o + ' == "' + t + '")'),
             `typeof ${o} == ${t}`);
  var other_types = types.filter((x) => x !== t);
  for (var other of other_types) {
    assertFalse(eval('run(()=>typeof ' + o + ' == "' + other + '")'),
                `typeof ${o} != ${other}`);
  }
}

testTypeOf('undefined', 'undefined');
testTypeOf('null', 'object');
testTypeOf('true', 'boolean');
testTypeOf('false', 'boolean');
testTypeOf('42.42', 'number');
testTypeOf('42', 'number');
testTypeOf('42n', 'bigint');
testTypeOf('"42"', 'string');
testTypeOf('Symbol(42)', 'symbol');
testTypeOf('{}', 'object');
testTypeOf('[]', 'object');
testTypeOf('new Proxy({}, {})', 'object');
testTypeOf('new Proxy([], {})', 'object');
testTypeOf('(_ => 42)', 'function');
testTypeOf('function() {}', 'function');
testTypeOf('function*() {}', 'function');
testTypeOf('async function() {}', 'function');
testTypeOf('async function*() {}', 'function');
testTypeOf('new Proxy(_ => 42, {})', 'function');
testTypeOf('class {}', 'function');
testTypeOf('Object', 'function');

// Binop
assertEquals(run((a,b)=>{return a+b}, 41, 1), 42);
assertEquals(run((a,b)=>{return a*b}, 21, 2), 42);
assertEquals(run((a)=>{return a+3}, 39), 42);
assertEquals(run((a,b)=>{return a&b}, 0x23, 0x7), 0x3);
assertEquals(run((a)=>{return a&0x7}, 0x23), 0x3);
assertEquals(run((a,b)=>{return a|b}, 0x23, 0x7), 0x27);
assertEquals(run((a)=>{return a|0x7}, 0x23), 0x27);
assertEquals(run((a,b)=>{return a^b}, 0x23, 0x7), 0x24);
assertEquals(run((a)=>{return a^0x7}, 0x23), 0x24);

// Unop
assertEquals(run((x)=>{return x++}, 41), 41);
assertEquals(run((x)=>{return ++x}, 41), 42);
assertEquals(run((x)=>{return x--}, 41), 41);
assertEquals(run((x)=>{return --x}, 41), 40);
assertEquals(run((x)=>{return !x}, 41), false);
assertEquals(run((x)=>{return ~x}, 41), ~41);

// Calls
function f0() { return 42; }
function f1(x) { return x; }
function f2(x, y) { return x + y; }
function f3(x, y, z) { return y + z; }
assertEquals(run(()=>{return f0()}), 42);
assertEquals(run(()=>{return f1(42)}), 42);
assertEquals(run(()=>{return f2(41, 1)}), 42);
assertEquals(run(()=>{return f3(1, 2, 40)}), 42);

// Mapped Arguments
function mapped_args() {
  return [arguments.length, ...arguments];
}
function mapped_args_dup(a,a) {
  return [arguments.length, ...arguments];
}
assertEquals(run(mapped_args, 1, 2, 3), [3,1,2,3]);
assertEquals(run(mapped_args_dup, 1, 2, 3), [3,1,2,3]);

// Unmapped Arguments
function unmapped_args() {
  "use strict";
  return [arguments.length, ...arguments];
}
assertEquals(run(unmapped_args, 1, 2, 3), [3,1,2,3]);

// Rest Arguments
function rest_args(...rest) {
  return [rest.length, ...rest];
}
assertEquals(run(rest_args, 1, 2, 3), [3,1,2,3]);

// Property call
let obj = {
  f0: () => { return 42; },
  f1: (x) => { return x; },
  f2: (x, y) => { return x + y; },
  f3: (x, y, z) => { return y + z; }
}
assertEquals(run(()=>{return obj.f0()}), 42);
assertEquals(run(()=>{return obj.f1(42)}), 42);
assertEquals(run(()=>{return obj.f2(41, 1)}), 42);
assertEquals(run(()=>{return obj.f3(1, 2, 40)}), 42);

// Call with spread
let ns = [2, 40];
assertEquals(run(()=>{return f3("x", ...ns)}), 42);

// Construct
function C(a, b, c) { this.x = 39 + b + c; }
assertEquals(run(()=>{return (new C("a", 1, 2)).x}), 42);
assertEquals(run(()=>{return (new C("a", ...ns)).x}), 81);

// Construct Array
assertEquals(run(()=>{return new Array(1, 2, 39);}).reduce((a,x)=>a+x), 42);

// Call Runtime
assertMatches(run(() => { return %NewRegExpWithBacktrackLimit("ax", "", 50); }), "ax");
run(() => { %CompileBaseline(()=>{}); });

// CallRuntimeForPair
assertEquals(run(()=>{with (f0) return f0();}), 42);

// Closure
assertEquals(run((o)=>{if (true) {let x = o; return ()=>x}}, 42)(), 42);
assertEquals(run((o)=>{return ()=>o}, 42)(), 42);

// Object / Array Literals
assertEquals(run((o)=>{return {a:42}}), {a:42});
assertEquals(run((o)=>{return [42]}), [42]);
assertEquals(run((o)=>{return []}), []);
assertEquals(run((o)=>{return {}}), {});
assertEquals(run((o)=>{return {...o}}, {a:42}), {a:42});
assertEquals(run((o)=>{return /42/}), /42/);
assertEquals(run((o)=>{return [...o]}, [1,2,3,4]), [1,2,3,4]);

// Construct
// Throw if the super() isn't a constructor
class T extends Object { constructor() { super() } }
T.__proto__ = null;
assertThrows(()=>construct(T));

run((o)=>{ try { } finally { } });

// SwitchOnSmiNoFeeback
run((o) => {
  var x = 0;
  var y = 0;
  while (true) {
    try {
      x++;
      if (x == 2) continue;
      if (x == 5) break;
    } finally {
      y++;
    }
  }
  return x + y;
}, 10);

// GetIterator
assertEquals(run((o)=>{
  let sum = 0; for (x of [1, 2]) {sum += x;} return sum;}), 3);

// ForIn
assertEquals(run((o)=>{ let sum = 0; for (let k in o) { sum += o[k] }; return sum }, {a:41,b:1}), 42);

// In
assertTrue(run((o, k)=>{return k in o}, {a:1}, "a"));
assertFalse(run((o, k)=>{return k in o}, {a:1}, "b"));

class D {}
assertTrue(run((o, c)=>{return o instanceof c}, new D(), D));
assertTrue(run((o, c)=>{return o instanceof c}, new D(), Object));
assertFalse(run((o, c)=>{return o instanceof c}, new D(), RegExp));

// CreateArrayFromIterable
assertEquals(run((a)=>{return [...a]}, [1,2,3]), [1,2,3]);

// Generator
let gen = run(function*() {
  yield 1;
  yield 2;
  yield 3;
});
let i = 1;
for (let val of gen) {
  assertEquals(i++, val);
}
assertEquals(4, i);

// Generator with a lot of locals
let gen_func_with_a_lot_of_locals = eval(`(function*() {
  ${ Array(32*1024).fill().map((x,i)=>`let local_${i};`).join("\n") }
  yield 1;
  yield 2;
  yield 3;
})`);
i = 1;
for (let val of run(gen_func_with_a_lot_of_locals)) {
  assertEquals(i++, val);
}
assertEquals(4, i);

// Async await
run(async function() {
  await 1;
  await 1;
  await 1;
  return 42;
}).then(x=>assertEquals(42, x));

// Try-catch
assertEquals(run((x)=>{
  if (x) {
    try {
      if (x) throw x;
      return 45;
    } catch (e) {
      return e;
    }
  }
}, 42), 42);

// Tier-up via InterpreterEntryTrampoline
(function() {
  function factory() {
    return function(a) {
      return a;
    };
  }
  let f1 = factory();
  let f2 = factory();
  %NeverOptimizeFunction(f1);
  %NeverOptimizeFunction(f2);

  assertEquals(f1(0), 0);
  assertEquals(f2(0), 0);
  assertTrue(isInterpreted(f1))
  assertFalse(isBaseline(f1));
  assertTrue(isInterpreted(f2))
  assertFalse(isBaseline(f2));

  %CompileBaseline(f1);
  assertEquals(f1(0), 0);
  assertTrue(isBaseline(f1));
  assertFalse(isBaseline(f2));

  assertEquals(f2(0), 0);
  assertTrue(isBaseline(f1));
  assertTrue(isBaseline(f2));
})();
