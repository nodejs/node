// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noanalyze-environment-liveness --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests scope chain inspection and scope objects.');

contextGroup.addScript(
    `
let topLevelLet = null;
var global_marker = 7;

function local_1() {
  debugger;
}

function local_2(a) {
  debugger;
}

function local_3(a) {
  var x = 3;
  debugger;
}

function local_4(a, b) {
  var x = 3;
  var y = 4;
  debugger;
}

function local_5() {
  eval('');
  debugger;
}

function local_6() {
  eval('var i = 5');
  debugger;
}

function local_7(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  debugger;
}

function with_1() {
  with({}) {
    debugger;
  }
}

function with_2() {
  with({}) {
    with({}) {
      debugger;
    }
  }
}

function with_3() {
  with({a:1, b:2}) {
    debugger;
  }
}

function with_4() {
  with({a:1, b:2}) {
    with({a:2, b:1}) {
      debugger;
    }
  }
}

var with_object = {c:3, d:4};
function with_5() {
  with(with_object) {
    with(with_object) {
      debugger;
    }
  }
}

function with_7() {
  with({}) {
    %OptimizeFunctionOnNextCall(with_7);
    debugger;
  }
}

function closure_1(a) {
  function f() {
    debugger;
    return a;
  }
  return f;
}

function closure_2(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    return a + x;
  }
  return f;
}

function closure_3(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    return a + b + x + y;
  }
  return f;
}

function closure_4(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    if (f) {
      return a + b + x + y;
    }
  }
  return f;
}

function closure_5(a, b) {
  var x = 3;
  var y = 4;
  function f() {
    eval('');
    debugger;
    return 1;
  }
  return f;
}

function closure_6(a, b) {
  function f(a, b) {
    var x = 3;
    var y = 4;
    return function() {
      var x = 3;
      var y = 4;
      debugger;
      some_global = a;
      return f;
    };
  }
  return f(a, b);
}

function closure_7(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  function f(a, b) {
    var x = 3;
    var y = 4;
    eval('var i = 5');
    eval('var j = 6');
    return function() {
      debugger;
      some_global = a;
      return f;
    };
  }
  return f(a, b);
}

function closure_8() {
  (function inner(x) {
    debugger;
  })(2);
}

function closure_9() {
  eval("var y = 1;");
  eval("var z = 1;");
  (function inner(x) {
    y++;
    z++;
    debugger;
  })(2);
}

function closure_1_inferred_name(a) {
  let foo = {};
  foo.bar = function() {
    debugger;
    return a;
  };
  return foo.bar;
}

function closure_2_inferred_name(a) {
  let foo = {};
  function FooBar(b) {
    foo.baz = function() {
      debugger;
      return a + b;
    };
    return foo.baz;
  }
  return FooBar;
}

function closure_3_inferred_name(a) {
  let foo = {};
  foo.bar = function(b) {
    foo.baz = function() {
      debugger;
      return a + b;
    };
    return foo.baz;
  };
  return foo.bar;
}

function closure_10(a) {
  var x = a + 2;
  function closure_11(b) {
    debugger;
    return a + b + x;
  }
  [42].forEach(closure_11);
}

function the_full_monty(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  function f(a, b) {
    var x = 9;
    var y = 10;
    eval('var i = 11');
    eval('var j = 12');
    with ({j:13}) {
      return function() {
        var x = 14;
        with ({a:15}) {
          with ({b:16}) {
            debugger;
            some_global = a;
            return f;
          }
        }
      };
    }
  }
  return f(a, b);
}

function closure_in_with_1() {
  with({x:1}) {
    (function inner(x) {
      debugger;
    })(2);
  }
}

function closure_in_with_2() {
  with({x:1}) {
    (function inner(x) {
      with({x:3}) {
        debugger;
      }
    })(2);
  }
}

function createClosure(a) {
  var b = a + 1;
  return function closure() {
    var c = b;
    (function inner(x) {
      with({x:c}) {
        debugger;
      }
    })(2);
  };
}

function closure_in_with_3() {
  var f = createClosure(0);
  f();
}

function catch_block_1() {
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
}

function catch_block_2() {
  try {
    throw 'Exception';
  } catch (e) {
    with({n:10}) {
      debugger;
    }
  }
}

function catch_block_3() {
  eval("var y = 78;");
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
}

function catch_block_4() {
  eval("var y = 98;");
  try {
    throw 'Exception';
  } catch (e) {
    with({n:10}) {
      debugger;
    }
  }
}

function catch_block_7() {
  %OptimizeFunctionOnNextCall(catch_block_7);
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
}

var code1 = \`function f() {
  var a = 1;
  function b() {
    debugger;
    return a + 1;
  }
  b();
}
f();
\`;

var code2 = \`function catch_block() {
  try {
    throw 'Exception';
  } catch (e) {
    with({n : 10}) {
      debugger;
    }
  }
}
catch_block();
\`;

var code3 = \`function for_statement() {
  for (let i = 0; i < 1; i++) {
    debugger;
  }
}
for_statement();
\`;

var code4 = \`function for_statement() {
  for (let i = 0; i < 1; i++) {
    let j;
    debugger;
  }
}
for_statement();
\`;

var code5 = \`function for_each_statement() {
  for (let i of [0]) {
    debugger;
  }
}
for_each_statement();
\`;

var code6 = \`function for_each_statement() {
  for (let i of [0]) {
    let j;
    debugger;
  }
}
for_each_statement();
\`;

var code7 = \`function for_each_statement() {
  var i;
  for (i of [0]) {
    debugger;
  }
}
for_each_statement();
\`;

var code8 = \`function for_each_statement() {
  var i;
  for (i of [0]) {
    let j;
    debugger;
  }
}
for_each_statement();
\`;

function switch_discriminant_test() {
  function tag() {
    debugger;
    return 0;
  }
  switch (tag()) {
    case 0:
      let x = 1;
      break;
  }
}
`,
    0, 0, 'functions.js');

(async function test() {
  await Protocol.Debugger.enable();

  let test_name = '';
  let listener_called = false;
  let exception = null;
  let begin_test_count = 0;
  let end_test_count = 0;
  let break_count = 0;

  function BeginTest(name) {
    test_name = name;
    listener_called = false;
    exception = null;
    begin_test_count++;
  }

  function EndTest() {
    if (!listener_called) {
      InspectorTest.log('listener not called for ' + test_name);
    }
    if (exception !== null) {
      InspectorTest.log(test_name + ' / ' + exception);
    }
    end_test_count++;
    InspectorTest.log(test_name + ' passed');
  }

  var ObjectPrototypeToString = Object.prototype.toString;
  var NumberPrototypeValueOf = Number.prototype.valueOf;
  var BooleanPrototypeValueOf = Boolean.prototype.valueOf;
  var StringPrototypeValueOf = String.prototype.valueOf;
  var DatePrototypeValueOf = Date.prototype.valueOf;
  var RegExpPrototypeToString = RegExp.prototype.toString;
  var BigIntPrototypeValueOf = BigInt.prototype.valueOf;

  function classOf(object) {
    var string = ObjectPrototypeToString.call(object);
    return string.substring(8, string.length - 1);
  }

  function ValueOf(value) {
    switch (classOf(value)) {
      case 'Number':
        return NumberPrototypeValueOf.call(value);
      case 'BigInt':
        return BigIntPrototypeValueOf.call(value);
      case 'String':
        return StringPrototypeValueOf.call(value);
      case 'Boolean':
        return BooleanPrototypeValueOf.call(value);
      case 'Date':
        return DatePrototypeValueOf.call(value);
      default:
        return value;
    }
  }

  var deepEquals;
  function deepObjectEquals(a, b) {
    var aProps = Object.getOwnPropertyNames(a);
    aProps.sort();
    var bProps = Object.getOwnPropertyNames(b);
    bProps.sort();
    if (!deepEquals(aProps, bProps)) {
      return false;
    }
    for (var i = 0; i < aProps.length; i++) {
      if (!deepEquals(a[aProps[i]], b[aProps[i]])) {
        return false;
      }
    }
    return true;
  }

  deepEquals = function deepEquals(a, b) {
    if (a === b) {
      if (a === 0) return (1 / a) === (1 / b);
      return true;
    }
    if (typeof a !== typeof b) return false;
    if (typeof a === 'number') return isNaN(a) && isNaN(b);
    if (typeof a !== 'object' && typeof a !== 'function') return false;
    var objectClass = classOf(a);
    if (objectClass !== classOf(b)) return false;
    switch (objectClass) {
      case 'RegExp':
        return RegExpPrototypeToString.call(a) ===
            RegExpPrototypeToString.call(b);
      case 'Function':
        return false;
      case 'Array':
        if (a.length !== b.length) return false;
        for (var i = 0; i < a.length; i++) {
          if ((i in a) !== (i in b)) return false;
          if (!deepEquals(a[i], b[i])) return false;
        }
        return true;
      case 'String':
      case 'Number':
      case 'BigInt':
      case 'Boolean':
      case 'Date':
        return ValueOf(a) === ValueOf(b);
    }
    return deepObjectEquals(a, b);
  };

  function propertiesToObject(props) {
    const obj = {};
    props.forEach((elem) => {
      const key = elem.name;
      let value;
      if (elem.value) {
        switch (elem.value.type) {
          case 'undefined':
            value = undefined;
            break;
          default:
            value = elem.value.value;
            break;
        }
      }
      obj[key] = value;
    });
    return obj;
  }

  function assertScopeMirrorEquals(scope1, scope2, props1, props2) {
    if (scope1.type !== scope2.type) {
      throw new Error('scopeType mismatch');
    }
    if (!deepObjectEquals(
            propertiesToObject(props1), propertiesToObject(props2))) {
      throw new Error('scopeObject properties mismatch');
    }
  }

  async function checkScopeChain(scopes, callFrame) {
    const scopeChain = callFrame.scopeChain;
    if (scopes.length !== scopeChain.length) {
      throw new Error(
          `Scope count mismatch: expected ${scopes.length}, ` +
          `got ${scopeChain.length}`);
    }
    for (let i = 0; i < scopes.length; i++) {
      const scope = scopeChain[i];
      if (scopes[i] !== scope.type) {
        throw new Error(
            `Scope ${i} type mismatch: expected ${scopes[i]}, ` +
            `got ${scope.type}`);
      }
      const {result: {result: props1}} = await Protocol.Runtime.getProperties({
        objectId: scope.object.objectId,
        ownProperties: true,
      });
      const {result: {result: props2}} = await Protocol.Runtime.getProperties({
        objectId: scope.object.objectId,
        ownProperties: true,
      });
      assertScopeMirrorEquals(scope, scope, props1, props2);
      if (scope.type === 'global') {
        if (propertiesToObject(props1).global_marker !== 7) {
          throw new Error('global_marker mismatch on global scope');
        }
      }
    }
  }

  async function checkScopeContent(minimum_content, number, callFrame) {
    const scope = callFrame.scopeChain[number];
    const {result: {result: props}} = await Protocol.Runtime.getProperties({
      objectId: scope.object.objectId,
      ownProperties: true,
    });
    const propMap = new Map();
    for (const elem of props) {
      let val = undefined;
      if (elem.value) {
        val = elem.value.type === 'undefined' ? undefined : elem.value.value;
      }
      propMap.set(elem.name, val);
    }

    let minimum_count = 0;
    for (const p in minimum_content) {
      if (!propMap.has(p)) {
        throw new Error(`property ${p} not found in scope`);
      }
      if (propMap.get(p) !== minimum_content[p]) {
        throw new Error(
            `property ${p} has unexpected value: ${propMap.get(p)} vs ` +
            minimum_content[p]);
      }
      minimum_count++;
    }

    let scope_size = props.length;
    if (propMap.has('arguments')) scope_size--;
    if (propMap.has('this')) scope_size--;
    if (propMap.has('')) {
      throw new Error('temporary empty property found');
    }
    if (scope_size < minimum_count) {
      throw new Error(
          `scope_size ${scope_size} < minimum_count ${minimum_count}`);
    }
  }

  function checkScopeChainNames(names, callFrame) {
    if (names.length !== callFrame.scopeChain.length) {
      throw new Error(
          `Names length mismatch: expected ${names.length}, ` +
          `got ${callFrame.scopeChain.length}`);
    }
  }

  function checkScopeChainPositions(positions, callFrame) {
    if (positions.length !== callFrame.scopeChain.length) {
      throw new Error(
          `Positions length mismatch: expected ${positions.length}, ` +
          `got ${callFrame.scopeChain.length}`);
    }
  }

  async function getScopeValues(scope) {
    const {result: {result: props}} = await Protocol.Runtime.getProperties({
      objectId: scope.object.objectId,
      ownProperties: true,
    });
    const obj = {};
    for (const elem of props) {
      if (elem.value) {
        obj[elem.name] =
            elem.value.type === 'undefined' ? undefined : elem.value.value;
      }
    }
    return obj;
  }

  let listener_delegate = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    break_count++;
    listener_called = true;
    try {
      if (listener_delegate) {
        await listener_delegate(callFrames);
      }
    } catch (e) {
      exception = e;
    }
    await Protocol.Debugger.resume();
  });

  // Local 1
  BeginTest('Local 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_1();'});
  EndTest();

  // Local 2
  BeginTest('Local 2');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_2(1);'});
  EndTest();

  // Local 3
  BeginTest('Local 3');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1, x: 3}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_3(1);'});
  EndTest();

  // Local 4
  BeginTest('Local 4');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1, b: 2, x: 3, y: 4}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_4(1, 2);'});
  EndTest();

  // Local 5
  BeginTest('Local 5');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_5();'});
  EndTest();

  // Local 6
  BeginTest('Local 6');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({i: 5}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_6();'});
  EndTest();

  // Local 7
  BeginTest('Local 7');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 3, y: 4, i: 5, j: 6}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'local_7(1, 2);'});
  EndTest();

  // With 1
  BeginTest('With 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['with', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'with_1();'});
  EndTest();

  // With 2
  BeginTest('With 2');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'with', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 0, callFrames[0]);
    await checkScopeContent({}, 1, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'with_2();'});
  EndTest();

  // With 3
  BeginTest('With 3');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['with', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1, b: 2}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'with_3();'});
  EndTest();

  // With 4
  BeginTest('With 4');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'with', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 2, b: 1}, 0, callFrames[0]);
    await checkScopeContent({a: 1, b: 2}, 1, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'with_4();'});
  EndTest();

  // With 5
  BeginTest('With 5');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'with', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({c: 3, d: 4}, 0, callFrames[0]);
    await checkScopeContent({c: 3, d: 4}, 1, callFrames[0]);
    const v0 = await getScopeValues(callFrames[0].scopeChain[0]);
    const v1 = await getScopeValues(callFrames[0].scopeChain[1]);
    if (v0.c !== 3 || v0.d !== 4 || v1.c !== 3 || v1.d !== 4) {
      throw new Error('With 5 value mismatch');
    }
  };
  await Protocol.Runtime.evaluate({expression: 'with_5();'});
  EndTest();

  // With 6
  BeginTest('With 6');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['with', 'with', 'script', 'global'], callFrames[0]);
    await checkScopeContent({c: 3, d: 4}, 0, callFrames[0]);
    await checkScopeContent({c: 3, d: 4}, 1, callFrames[0]);
    const v0 = await getScopeValues(callFrames[0].scopeChain[0]);
    const v1 = await getScopeValues(callFrames[0].scopeChain[1]);
    if (v0.c !== 3 || v0.d !== 4 || v1.c !== 3 || v1.d !== 4) {
      throw new Error('With 6 value mismatch');
    }
  };
  await Protocol.Runtime.evaluate({
    expression: 'with(with_object) { with(with_object) { debugger; } }',
  });
  EndTest();

  // With 7
  BeginTest('With 7');
  await Protocol.Runtime.evaluate({
    expression: '%PrepareFunctionForOptimization(with_7);',
  });
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['with', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 0, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'with_7();'});
  EndTest();

  // Closure 1
  BeginTest('Closure 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1}, 1, callFrames[0]);
    checkScopeChainNames(
        ['f', 'closure_1', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_1(1)();'});
  EndTest();

  // Closure 2
  BeginTest('Closure 2');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1, x: 3}, 1, callFrames[0]);
    checkScopeChainNames(
        ['f', 'closure_2', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_2(1, 2)();'});
  EndTest();

  // Closure 3
  BeginTest('Closure 3');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1, b: 2, x: 3, y: 4}, 1, callFrames[0]);
    checkScopeChainNames(
        ['f', 'closure_3', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_3(1, 2)();'});
  EndTest();

  // Closure 4
  BeginTest('Closure 4');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 3, y: 4, f: undefined}, 1, callFrames[0]);
    checkScopeChainNames(
        ['f', 'closure_4', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_4(1, 2)();'});
  EndTest();

  // Closure 5
  BeginTest('Closure 5');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 3, y: 4, f: undefined}, 1, callFrames[0]);
    checkScopeChainNames(
        ['f', 'closure_5', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_5(1, 2)();'});
  EndTest();

  // Closure 6
  BeginTest('Closure 6');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1}, 1, callFrames[0]);
    await checkScopeContent({f: undefined}, 2, callFrames[0]);
    checkScopeChainNames(
        [undefined, 'f', 'closure_6', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_6(1, 2)();'});
  EndTest();

  // Closure 7
  BeginTest('Closure 7');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 0, callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 3, y: 4, i: 5, j: 6}, 1, callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 3, y: 4, i: 5, j: 6, f: undefined}, 2, callFrames[0]);
    checkScopeChainNames(
        [undefined, 'f', 'closure_7', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_7(1, 2)();'});
  EndTest();

  // Closure 8
  BeginTest('Closure 8');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({x: 2}, 0, callFrames[0]);
    await checkScopeContent({}, 1, callFrames[0]);
    checkScopeChainNames(
        ['inner', undefined, undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_8();'});
  EndTest();

  // Closure 9
  BeginTest('Closure 9');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    checkScopeChainNames(
        ['inner', 'closure_9', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_9();'});
  EndTest();

  // Closure with Inferred Name 1
  BeginTest('Closure with Inferred Name 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({a: 1}, 1, callFrames[0]);
    checkScopeChainNames(
        ['foo.bar', 'closure_1_inferred_name', undefined, undefined],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: 'closure_1_inferred_name(1)();',
  });
  EndTest();

  // Closure with Inferred Name 2
  BeginTest('Closure with Inferred Name 2');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({b: 0x1235}, 1, callFrames[0]);
    await checkScopeContent({a: 0x1234}, 2, callFrames[0]);
    checkScopeChainNames(
        [
          'FooBar.foo.baz', 'FooBar', 'closure_2_inferred_name', undefined,
          undefined
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: 'closure_2_inferred_name(0x1234)(0x1235)();',
  });
  EndTest();

  // Closure with Inferred Name 3
  BeginTest('Closure with Inferred Name 3');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({b: 0x1235}, 1, callFrames[0]);
    await checkScopeContent({a: 0x1234}, 2, callFrames[0]);
    checkScopeChainNames(
        ['foo.baz', 'foo.bar', 'closure_3_inferred_name', undefined, undefined],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: 'closure_3_inferred_name(0x1234)(0x1235)();',
  });
  EndTest();

  // Closure passed to optimized Array.prototype.forEach
  BeginTest('Closure passed to optimized Array.prototype.forEach');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({b: 42}, 0, callFrames[0]);
    await checkScopeContent({a: 5, x: 7}, 1, callFrames[0]);
    checkScopeChainNames(
        ['closure_11', 'closure_10', undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: '%PrepareFunctionForOptimization(closure_10);',
  });
  begin_test_count++;
  await Protocol.Runtime.evaluate({expression: 'closure_10(5);'});
  end_test_count++;
  begin_test_count++;
  await Protocol.Runtime.evaluate({expression: 'closure_10(5);'});
  end_test_count++;
  await Protocol.Runtime.evaluate({
    expression: '%OptimizeFunctionOnNextCall(closure_10);',
  });
  await Protocol.Runtime.evaluate({expression: 'closure_10(5);'});
  EndTest();

  // The full monty
  BeginTest('The full monty');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        [
          'with', 'with', 'local', 'with', 'closure', 'closure', 'script',
          'global'
        ],
        callFrames[0]);
    await checkScopeContent({b: 16}, 0, callFrames[0]);
    await checkScopeContent({a: 15}, 1, callFrames[0]);
    await checkScopeContent({x: 14}, 2, callFrames[0]);
    await checkScopeContent({j: 13}, 3, callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 9, y: 10, i: 11, j: 12}, 4, callFrames[0]);
    await checkScopeContent(
        {a: 1, b: 2, x: 3, y: 4, i: 5, j: 6, f: undefined}, 5, callFrames[0]);
    checkScopeChainNames(
        [
          undefined, undefined, undefined, 'f', 'f', 'the_full_monty',
          undefined, undefined
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'the_full_monty(1, 2)();'});
  EndTest();

  // Closure inside With 1
  BeginTest('Closure inside With 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'with', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({x: 2}, 0, callFrames[0]);
    await checkScopeContent({x: 1}, 1, callFrames[0]);
    await checkScopeContent({}, 2, callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_in_with_1();'});
  EndTest();

  // Closure inside With 2
  BeginTest('Closure inside With 2');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'local', 'with', 'closure', 'script', 'global'],
        callFrames[0]);
    await checkScopeContent({x: 3}, 0, callFrames[0]);
    await checkScopeContent({x: 2}, 1, callFrames[0]);
    await checkScopeContent({x: 1}, 2, callFrames[0]);
    await checkScopeContent({}, 3, callFrames[0]);
    checkScopeChainNames(
        [
          'inner', 'inner', 'closure_in_with_2', undefined, undefined, undefined
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_in_with_2();'});
  EndTest();

  // Closure inside With 3
  BeginTest('Closure inside With 3');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'local', 'closure', 'closure', 'script', 'global'],
        callFrames[0]);
    checkScopeChainNames(
        ['inner', 'inner', 'closure', 'createClosure', undefined, undefined],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'closure_in_with_3();'});
  EndTest();

  // Closure inside With 4
  BeginTest('Closure inside With 4');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'with', 'script', 'global'], callFrames[0]);
    await checkScopeContent({x: 2}, 0, callFrames[0]);
    await checkScopeContent({x: 1}, 1, callFrames[0]);
    checkScopeChainNames(
        [undefined, undefined, undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: 'with({x:1}) { (function(x) { debugger; })(2); }',
  });
  EndTest();

  // Global
  BeginTest('Global');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['script', 'global'], callFrames[0]);
    checkScopeChainNames([undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'debugger;'});
  EndTest();

  // Catch block 1
  BeginTest('Catch block 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['catch', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 0, callFrames[0]);
    checkScopeChainNames(
        ['catch_block_1', 'catch_block_1', undefined, undefined],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'catch_block_1();'});
  EndTest();

  // Catch block 2
  BeginTest('Catch block 2');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'catch', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({n: 10}, 0, callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 1, callFrames[0]);
    checkScopeChainNames(
        [
          'catch_block_2', 'catch_block_2', 'catch_block_2', undefined,
          undefined
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'catch_block_2();'});
  EndTest();

  // Catch block 3
  BeginTest('Catch block 3');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['catch', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 0, callFrames[0]);
    await checkScopeContent({y: 78}, 1, callFrames[0]);
    checkScopeChainNames(
        ['catch_block_3', 'catch_block_3', undefined, undefined],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'catch_block_3();'});
  EndTest();

  // Catch block 4
  BeginTest('Catch block 4');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['with', 'catch', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({n: 10}, 0, callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 1, callFrames[0]);
    await checkScopeContent({y: 98}, 2, callFrames[0]);
    checkScopeChainNames(
        [
          'catch_block_4', 'catch_block_4', 'catch_block_4', undefined,
          undefined
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'catch_block_4();'});
  EndTest();

  // Catch block 5
  BeginTest('Catch block 5');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['catch', 'script', 'global'], callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 0, callFrames[0]);
    checkScopeChainNames([undefined, undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: 'try { throw "Exception"; } catch (e) { debugger; }',
  });
  EndTest();

  // Catch block 6
  BeginTest('Catch block 6');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'catch', 'script', 'global'], callFrames[0]);
    await checkScopeContent({x: 2}, 0, callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 1, callFrames[0]);
    checkScopeChainNames(
        [undefined, undefined, undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: 'try { throw "Exception"; } catch (e) {' +
        ' (function(x) { debugger; })(2); }',
  });
  EndTest();

  // Catch block 7
  BeginTest('Catch block 7');
  await Protocol.Runtime.evaluate({
    expression: '%PrepareFunctionForOptimization(catch_block_7);',
  });
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['catch', 'local', 'script', 'global'], callFrames[0]);
    await checkScopeContent({e: 'Exception'}, 0, callFrames[0]);
    checkScopeChainNames(
        ['catch_block_7', 'catch_block_7', undefined, undefined],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'catch_block_7();'});
  EndTest();

  // Classes and methods 1
  BeginTest('Classes and methods 1');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['local', 'block', 'closure', 'script', 'global'], callFrames[0]);
    await checkScopeContent({}, 3, callFrames[0]);
    checkScopeChainNames(
        ['m', undefined, undefined, undefined, undefined], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({
    expression: `(function() {
      "use strict";
      class C1 {
        m() {
          debugger;
        }
      }
      new C1().m();
    })();`,
  });
  EndTest();

  // Scope positions
  BeginTest('Scope positions');
  listener_delegate = async (callFrames) => {
    checkScopeChainPositions(
        [{start: 58, end: 118}, {start: 10, end: 162}, {}, {}], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code1);'});
  EndTest();

  // Scope positions in catch and 'with' statement
  BeginTest('Scope positions in catch and \'with\' statement');
  listener_delegate = async (callFrames) => {
    checkScopeChainPositions(
        [
          {start: 131, end: 173}, {start: 94, end: 199}, {start: 20, end: 225},
          {}, {}
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code2);'});
  EndTest();

  // Scope positions in for statement
  BeginTest('Scope positions in for statement');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['block', 'local', 'script', 'global'], callFrames[0]);
    checkScopeChainPositions(
        [{start: 42, end: 111}, {start: 22, end: 145}, {}, {}], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code3);'});
  EndTest();

  // Scope positions in for statement with lexical block
  BeginTest('Scope positions in for statement with lexical block');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['block', 'block', 'local', 'script', 'global'], callFrames[0]);
    checkScopeChainPositions(
        [
          {start: 66, end: 147}, {start: 42, end: 147}, {start: 22, end: 181},
          {}, {}
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code4);'});
  EndTest();

  // Scope positions in lexical for each statement
  BeginTest('Scope positions in lexical for each statement');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['block', 'local', 'script', 'global'], callFrames[0]);
    checkScopeChainPositions(
        [{start: 55, end: 111}, {start: 27, end: 145}, {}, {}], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code5);'});
  EndTest();

  // Scope positions in lexical for each statement with lexical block
  BeginTest('Scope positions in lexical for each statement with lexical block');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['block', 'block', 'local', 'script', 'global'], callFrames[0]);
    checkScopeChainPositions(
        [
          {start: 57, end: 147}, {start: 55, end: 147}, {start: 27, end: 181},
          {}, {}
        ],
        callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code6);'});
  EndTest();

  // Scope positions in non-lexical for each statement
  BeginTest('Scope positions in non-lexical for each statement');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(['local', 'script', 'global'], callFrames[0]);
    checkScopeChainPositions([{start: 27, end: 181}, {}, {}], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code7);'});
  EndTest();

  // Scope positions in non-lexical for each statement with lexical block
  BeginTest(
      'Scope positions in non-lexical for each statement with lexical block');
  listener_delegate = async (callFrames) => {
    await checkScopeChain(
        ['block', 'local', 'script', 'global'], callFrames[0]);
    checkScopeChainPositions(
        [{start: 89, end: 183}, {start: 27, end: 217}, {}, {}], callFrames[0]);
  };
  await Protocol.Runtime.evaluate({expression: 'eval(code8);'});
  EndTest();

  // Switch discriminant is outside switch block scope
  BeginTest('Switch discriminant is outside switch block scope');
  listener_delegate = async (callFrames) => {
    const outer_frame = callFrames[1];
    if (outer_frame.scopeChain.length !== 3) {
      throw new Error(
          `Outer frame scope count mismatch: ${outer_frame.scopeChain.length}`);
    }
    if (outer_frame.scopeChain[0].type !== 'local' ||
        outer_frame.scopeChain[1].type !== 'script' ||
        outer_frame.scopeChain[2].type !== 'global') {
      throw new Error(
          `Outer frame scopes unexpected: ` +
          outer_frame.scopeChain.map(s => s.type).join(','));
    }
  };
  await Protocol.Runtime.evaluate({expression: 'switch_discriminant_test();'});
  EndTest();

  if (begin_test_count !== break_count) {
    InspectorTest.log(
        `Break count mismatch: ${begin_test_count} vs ${break_count}`);
  }
  if (begin_test_count !== end_test_count) {
    InspectorTest.log(
        `End test count mismatch: ${begin_test_count} vs ${end_test_count}`);
  }

  InspectorTest.log(
      `All ${begin_test_count} tests completed successfully ` +
      `(${break_count} breaks).`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
