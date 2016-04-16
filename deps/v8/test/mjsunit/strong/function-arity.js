// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --harmony-reflect
// Flags: --allow-natives-syntax

'use strict';


function generateArguments(n, prefix) {
  let a = [];
  if (prefix) {
    a.push(prefix);
  }
  for (let i = 0; i < n; i++) {
    a.push(String(i));
  }

  return a.join(', ');
}


function generateParams(n, directive_in_body) {
  let a = [];
  for (let i = 0; i < n; i++) {
    a[i] = `p${i}`;
  }
  return a.join(', ');
}

function generateParamsWithRest(n, directive_in_body) {
  let a = [];
  let i = 0;
  for (; i < n; i++) {
    a[i] = `p${i}`;
  }
  if (!directive_in_body) {
    // If language mode directive occurs in body, rest parameters will trigger
    // an early error regardless of language mode.
    a.push(`...p${i}`);
  }
  return a.join(', ');
}


function generateSpread(n) {
  return `...[${generateArguments(n)}]`;
}


(function FunctionCall() {
  for (let parameterCount = 0; parameterCount < 3; parameterCount++) {
    let defs = [
      `'use strong'; function f(${generateParams(parameterCount)}) {}`,
      `'use strong'; function f(${generateParamsWithRest(parameterCount)}) {}`,
      `'use strong'; function* f(${generateParams(parameterCount)}) {}`,
      `'use strong'; function* f(${generateParamsWithRest(parameterCount)}) {}`,
      `'use strong'; let f = (${generateParams(parameterCount)}) => {}`,
      `function f(${generateParams(parameterCount)}) { 'use strong'; }`,
      `function* f(${generateParams(parameterCount)}) { 'use strong'; }`,
      `let f = (${generateParams(parameterCount)}) => { 'use strong'; }`,
    ];
    for (let def of defs) {
      for (let argumentCount = 0; argumentCount < 3; argumentCount++) {
        let calls = [
          `f(${generateArguments(argumentCount)})`,
          `f(${generateSpread(argumentCount)})`,
          `f.call(${generateArguments(argumentCount, 'undefined')})`,
          `f.call(undefined, ${generateSpread(argumentCount)})`,
          `f.apply(undefined, [${generateArguments(argumentCount)}])`,
          `f.bind(undefined)(${generateArguments(argumentCount)})`,
          `%_Call(f, ${generateArguments(argumentCount, 'undefined')})`,
          `%Call(f, ${generateArguments(argumentCount, 'undefined')})`,
          `%Apply(f, undefined, [${generateArguments(argumentCount)}], 0,
                  ${argumentCount})`,
        ];

        for (let call of calls) {
          let code = `'use strict'; ${def}; ${call};`;
          if (argumentCount < parameterCount) {
            print(code);
            assertThrows(code, TypeError);
          } else {
            assertDoesNotThrow(code);
          }
        }
      }

      let calls = [
        `f.call()`,
        `f.apply()`,
        `f.apply(undefined)`,
      ];
      for (let call of calls) {
        let code = `'use strict'; ${def}; ${call};`;
        if (parameterCount > 0) {
          assertThrows(code, TypeError);
        } else {
          assertDoesNotThrow(code);
        }
      }
    }
  }
})();


(function MethodCall() {
  for (let genParams of [generateParams, generateParamsWithRest]) {
    for (let parameterCount = 0; parameterCount < 3; parameterCount++) {
      let defs = [
        `let o = new class {
          m(${genParams(parameterCount, true)}) { 'use strong'; }
        }`,
        `let o = new class {
          *m(${genParams(parameterCount, true)}) { 'use strong'; }
        }`,
        `let o = { m(${genParams(parameterCount, true)}) { 'use strong'; } }`,
        `let o = { *m(${genParams(parameterCount, true)}) { 'use strong'; } }`,
        `'use strong';
        let o = new class { m(${genParams(parameterCount)}) {} }`,
        `'use strong';
        let o = new class { *m(${genParams(parameterCount)}) {} }`,
        `'use strong'; let o = { m(${genParams(parameterCount)}) {} }`,
        `'use strong'; let o = { *m(${genParams(parameterCount)}) {} }`,
      ];
      for (let def of defs) {
        for (let argumentCount = 0; argumentCount < 3; argumentCount++) {
          let calls = [
            `o.m(${generateArguments(argumentCount)})`,
            `o.m(${generateSpread(argumentCount)})`,
            `o.m.call(${generateArguments(argumentCount, 'o')})`,
            `o.m.call(o, ${generateSpread(argumentCount)})`,
            `o.m.apply(o, [${generateArguments(argumentCount)}])`,
            `o.m.bind(o)(${generateArguments(argumentCount)})`,
            `%_Call(o.m, ${generateArguments(argumentCount, 'o')})`,
            `%Call(o.m, ${generateArguments(argumentCount, 'o')})`,
            `%Apply(o.m, o, [${generateArguments(argumentCount)}], 0,
                    ${argumentCount})`,
          ];

          for (let call of calls) {
            let code = `'use strict'; ${def}; ${call};`;
            if (argumentCount < parameterCount) {
              assertThrows(code, TypeError);
            } else {
              assertDoesNotThrow(code);
            }
          }
        }

        let calls = [
          `o.m.call()`,
          `o.m.apply()`,
          `o.m.apply(o)`,
        ];
        for (let call of calls) {
          let code = `'use strict'; ${def}; ${call};`;
          if (parameterCount > 0) {
            assertThrows(code, TypeError);
          } else {
            assertDoesNotThrow(code);
          }
        }
      }
    }
  }
})();


(function Constructor() {
  for (let genParams of [generateParams, generateParamsWithRest]) {
    for (let argumentCount = 0; argumentCount < 3; argumentCount++) {
      for (let parameterCount = 0; parameterCount < 3; parameterCount++) {
        let defs = [
          `'use strong';
          class C { constructor(${genParams(parameterCount)}) {} }`,
        ];
        for (let def of defs) {
          let calls = [
            `new C(${generateArguments(argumentCount)})`,
            `new C(${generateSpread(argumentCount)})`,
            `Reflect.construct(C, [${generateArguments(argumentCount)}])`,
          ];
          for (let call of calls) {
            let code = `${def}; ${call};`;
            if (argumentCount < parameterCount) {
              assertThrows(code, TypeError);
            } else {
              assertDoesNotThrow(code);
            }
          }
        }
      }
    }
  }
})();


(function DerivedConstructor() {
  for (let genParams of [generateParams, generateParamsWithRest]) {
    for (let genArgs of [generateArguments, generateSpread]) {
      for (let argumentCount = 0; argumentCount < 3; argumentCount++) {
        for (let parameterCount = 0; parameterCount < 3; parameterCount++) {
          let defs = [
            `'use strong';
            class B {
              constructor(${genParams(parameterCount)}) {}
            }
            class C extends B {
              constructor() {
                super(${genArgs(argumentCount)});
              }
            }`,
          ];
          for (let def of defs) {
            let code = `${def}; new C();`;
            if (argumentCount < parameterCount) {
              assertThrows(code, TypeError);
            } else {
              assertDoesNotThrow(code);
            }
          }
        }
      }
    }
  }
})();


(function DerivedConstructorDefaultConstructorInDerivedClass() {
  for (let genParams of [generateParams, generateParamsWithRest]) {
    for (let genArgs of [generateArguments, generateSpread]) {
      for (let argumentCount = 0; argumentCount < 3; argumentCount++) {
        for (let parameterCount = 0; parameterCount < 3; parameterCount++) {
          let defs = [
            `'use strong';
            class B {
              constructor(${genParams(parameterCount)}) {}
            }
            class C extends B {}`,
          ];
          for (let def of defs) {
            let code = `${def}; new C(${genArgs(argumentCount)})`;
            if (argumentCount < parameterCount) {
              assertThrows(code, TypeError);
            } else {
              assertDoesNotThrow(code);
            }
          }
        }
      }
    }
  }
})();


(function TestOptimized() {
  function f(x, y) { 'use strong'; }

  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);

  function g() {
    f(1);
  }
  assertThrows(g, TypeError);
  %OptimizeFunctionOnNextCall(g);
  assertThrows(g, TypeError);

  f(1, 2);
  %OptimizeFunctionOnNextCall(f);
  f(1, 2);
})();


(function TestOptimized2() {
  'use strong';
  function f(x, y) {}

  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);

  function g() {
    f(1);
  }
  assertThrows(g, TypeError);
  %OptimizeFunctionOnNextCall(g);
  assertThrows(g, TypeError);

  f(1, 2);
  %OptimizeFunctionOnNextCall(f);
  f(1, 2);
})();


(function TestOptimized3() {
  function f(x, y) {}
  function g() {
    'use strong';
    f(1);
  }

  g();
  %OptimizeFunctionOnNextCall(f);
  g();
})();


(function ParametersSuper() {
  for (let genArgs of [generateArguments, generateSpread]) {
    for (let argumentCount = 0; argumentCount < 3; argumentCount++) {
      for (let parameterCount = 0; parameterCount < 3; parameterCount++) {
        let defs = [
          `'use strict';
          class B {
            m(${generateParams(parameterCount)} ){ 'use strong' }
          }`,
          `'use strong'; class B { m(${generateParams(parameterCount)}) {} }`,
        ];
        for (let def of defs) {
          let code = `${def};
              class D extends B {
                m() {
                  super.m(${genArgs(argumentCount)});
                }
              }
              new D().m()`;
          print('\n\n' + code);
          if (argumentCount < parameterCount) {
            assertThrows(code, TypeError);
          } else {
            assertDoesNotThrow(code);
          }
        }
      }
    }
  }
})();
