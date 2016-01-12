//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function* makeValueGen(a, b) {
  // return a for profiling
  yield a;
  // return b to bailout
  yield b;
  // return b again to compare with non jit function
  yield b;
}

function makeTest(name, config) {
  const f1 = `function ${name}(arr) {
    for(var i = -5; i < 15; ++i) {arr[i] = ${config.v1};}
    return arr;
  }`;
  const f2 = customName => `function ${customName}P(arr, v) {
    for(var i = 1; i < 8; ++i) {arr[i] = v;}
    return arr;
  }`;
  const f3 = `function ${name}V(arr) {
    const v = ${config.v1};
    for(var i = -2; i < 17; ++i) {arr[i] = v;}
    return arr;
  }`;

  const extraTests = (config.wrongTypes || []).map((wrongType, i) => {
    const difValue = {f: f2(`${name}W${i}`), compare: f2(`${name}WC${i}`)};
    const genValue = makeValueGen(config.v1, wrongType);
    Reflect.defineProperty(difValue, "v", {
      get: () => genValue.next().value
    });
    return difValue;
  });

  const tests = [
    {f: f1},
    {f: f2(name), v: config.v2 !== undefined ? config.v2 : config.v1},
    {f: f3},
  ].concat(extraTests);

  const convertTest = function(fnText) {
    var fn;
    eval(`fn = ${fnText}`);
    return fn;
  };
  for(const t of tests) {
    t.f = convertTest(t.f);
    t.compare = t.compare && convertTest(t.compare);
  }
  return tests;
}

const allTypes = [0, 1.5, undefined, null, 9223372036854775807, "string", {a: null, b: "b"}];
const tests = [
  {name: "memsetUndefined", v1: undefined, wrongTypes: allTypes},
  {name: "memsetNull", v1: null, wrongTypes: allTypes},
  {name: "memsetFloat", v1: 3.14, v2: -87.684, wrongTypes: allTypes},
  {name: "memsetNumber", v1: 9223372036854775807, v2: -987654987654987, wrongTypes: allTypes},
  {name: "memsetBoolean", v1: true, v2: false, wrongTypes: allTypes},
  {name: "memsetString", v1: "\"thatString\"", v2: "`A template string`", wrongTypes: allTypes},
  {name: "memsetObject", v1: "{test: 1}", v2: [1, 2, 3], wrongTypes: allTypes},
];

const types = "Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array Array".split(" ");
const global = this;

let passed = true;
for(const test of tests) {
  for(const t of types) {
    const fns = makeTest(`${test.name}${t}`, test);
    for(const detail of fns) {
      const fn = detail.f;
      let a1 = fn(new global[t](10), detail.v);
      const a2 = fn(new global[t](10), detail.v);
      if(detail.compare) {
        // the optimized version ran with a different value. Run again with a clean function to compare
        // reuse a1 to test if we correctly overwrite the values
        a1 = detail.compare(a1, detail.v);
      }
      if(a1.length !== a2.length) {
        passed = false;
        print(`${fn.name} (${t}) didn't return arrays with same length`);
        continue;
      }
      for(let i = 0; i < a1.length; ++i) {
        if(a1[i] !== a2[i] && !(isNaN(a1[i]) && isNaN(a2[i]))) {
          passed = false;
          print(`${fn.name} (${t}): a1[${i}](${a1[i]}) != a2[${i}](${a2[i]})`);
          break;
        }
      }
    }
  }
}

function memsetSymbol() {const s = Symbol(); const arr = new Array(10); for(let i = 0; i < 10; ++i) {arr[i] = s;} return arr;}
function memsetSymbolV(v) {const arr = new Array(10); for(let i = 0; i < 10; ++i) {arr[i] = v;} return arr;}
function checkSymbols() {
  const s = Symbol();
  // Since symbol are unique, and we want to compare the result, we have to pass the same symbol everytime
  const a1 = memsetSymbolV(s);
  const a2 = memsetSymbolV(s);
  for(let i = 0; i < a1.length; ++i) {
    if(a1[i] !== a2[i]) {
      passed = false;
      // need explicit toString() for Symbol
      print(`memsetSymbolV: a1[${i}](${a1[i].toString()}) != a2[${i}](${a2 && a2[i].toString() || ""})`);
      break;
    }
  }

  memsetSymbol();
  const symbolArray = memsetSymbol();
  for(let i = 0; i < symbolArray.length; ++i) {
    if(typeof symbolArray[i] !== typeof s) {
      passed = false;
      print(`memsetSymbol: symbolArray[${i}] is not a Symbol`);
      break;
    }
  }
}
checkSymbols();

print(passed ? "PASSED" : "FAILED");
