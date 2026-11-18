// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-staging

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-runtime-check.');

const source = `
var a = {name: 'foo'};
function set_name(a) {
  a.name = 'bar';
  return a.name;
}

var name_value = 'value';
function create_object_literal() {
  var obj = {name: name_value};
  return obj.name;
};

var arrayValue = 1;
function create_array_literal() {
  return [arrayValue];
}
var b = { 1: 2 };

function return_array_use_spread(a) {
  return [...a];
}

var array = [1,2,3];

function return_literal_with_data_property(a) {
  return {[a] : 1};
}

var set = new Set([1,2]);
var setIterator = set.entries();

var arrayIterator = array.entries();

var map = new Map([[1,2]]);
var mapIterator = map.entries();

var regExp = /a/;

let disposable_stack = new DisposableStack();
let async_disposable_stack = new AsyncDisposableStack();
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();

  async function success(expectation, source) {
    const {result} = await Protocol.Runtime.evaluate({
      expression: source,
      throwOnSideEffect: true,
      returnByValue: true,
    });
    if (result.exceptionDetails) {
      InspectorTest.log(source + ' threw: ' + result.result.description);
      return;
    }
    const actual = result.result.value !== undefined ?
        JSON.stringify(result.result.value) :
        result.result.unserializableValue;
    InspectorTest.log(source + ': ' + actual);
  }

  async function fail(source) {
    const {result} = await Protocol.Runtime.evaluate({
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(
        source + ': ' +
        (result.exceptionDetails ? result.result.className : 'succeeded'));
  }

  // StaCurrentContextSlotNoCell
  await success(10, `(function(){
  const x = 10;
  function f1() {return x;}
  return x;
})()`);

  // SetNamedProperty
  await fail(`set_name(a)`);
  await success('bar', `set_name({name: 'foo'})`);

  // DefineNamedOwnProperty
  await success('value', `create_object_literal()`);

  // SetKeyedProperty
  await success([1], `create_array_literal()`);
  await fail(`b[1] ^= 2`);

  // StaInArrayLiteral
  await success([1], `return_array_use_spread([1])`);

  // CallAccessorSetter
  await fail(`array.length = 2`);
  await success(2, `[1,2,3].length = 2`);

  // DefineKeyedOwnPropertyInLiteral
  await success({foo: 1}, `return_literal_with_data_property('foo')`);

  // Set builtins with temporary objects
  await fail(`set.add(3).size`);
  await success(1, `new Set().add(1).size`);

  await success(
      0, `(() => { const s = new Set([1]); s.delete(1); return s.size; })()`);
  await fail(`set.delete(1)`);

  await success(
      0, `(() => { const s = new Set([1]); s.clear(); return s.size; })()`);
  await fail(`set.clear()`);

  // new set
  await success(3, `(() => {
  let s = 0;
  for (const a of new Set([1,2]))
    s += a;
  return s;
})()`);
  // existing set
  await success(3, `(() => {
  let s = 0;
  for (const a of set)
    s += a;
  return s;
})()`);
  // existing iterator
  await fail(`(() => {
  let s = 0;
  for (const a of setIterator)
    s += a;
  return s;
})()`);

  // Array builtins with temporary objects
  await success([1, 1, 1], '[1,2,3].fill(1)');
  await fail(`array.fill(1)`);

  await success([1], `(() => { const a = []; a.push(1); return a; })()`);
  await fail(`array.push(1)`);

  await success([1], `(() => { const a = [1,2]; a.pop(); return a; })()`);
  await fail(`array.pop()`);

  await success([3, 2, 1], `[1,2,3].reverse()`);
  await fail(`array.reverse()`);

  await success([1, 2, 3], `[2,1,3].sort()`);
  await fail(`array.sort()`);

  await success([2, 3], `[1,2,3].splice(1,2)`);
  await fail(`array.splice(1,2)`);

  await success([1, 2], `(() => { const a = [2]; a.unshift(1); return a; })()`);
  await fail(`array.unshift(1)`);
  await success(1, `[1,2].shift()`);
  await fail(`array.shift()`);

  // new array
  await success(6, `(() => {
  let s = 0;
  for (const a of [1,2,3])
    s += a;
  return s;
})()`);
  // existing array
  await success(6, `(() => {
  let s = 0;
  for (const a of array)
    s += a;
  return s;
})()`);
  // existing iterator
  await fail(`(() => {
  let s = 0;
  for (const a of arrayIterator)
    s += a;
  return s;
})()`);

  await success(6, `array.reduce((a,b) => a + b, 0)`);

  // Map builtins with temporary objects
  await fail(`map.set(3, 4).size`);
  await success(1, `new Map().set(1, 2).size`);

  await success(0, `(() => {
  const m = new Map([[1, 2]]);
  m.delete(1);
  return m.size;
})()`);
  await fail(`map.delete(1)`);

  await success(0, `(() => {
  const m = new Map([[1, 2]]);
  m.clear();
  return m.size;
})()`);
  await fail(`map.clear()`);

  // new set
  await success(2, `(() => {
  let s = 0;
  for (const [a, b] of new Map([[1,2]]))
    s += b;
  return s;
})()`);
  // existing set
  await success(2, `(() => {
  let s = 0;
  for (const [a,b] of map)
    s += b;
  return s;
})()`);
  // existing iterator
  await fail(`(() => {
  let s = 0;
  for (const [a,b] of mapIterator)
    s += a;
  return s;
})()`);

  // Regexps
  await success(true, `/a/.test('a')`);
  await fail(`/a/.test({toString: () => {map.clear(); return 'a';}})`);
  await fail(`regExp.test('a')`);

  // DisposableStack use().
  await fail(`(() => {
  const disposable = {
    value: 1,
    [Symbol.dispose]() {
      return 43;
    }
  };
  disposable_stack.use(disposable);
})()`);
  await success(42, `(() => {
  let stack = new DisposableStack();
  const disposable = {
    value: 1,
    [Symbol.dispose]() {
      return 43;
    }
  };
  stack.use(disposable);
  return 42;
})()`);

  // DisposableStack dispose().
  await fail(`disposable_stack.dispose()`);
  await success(42, `(() => {
  let stack = new DisposableStack();
  stack.dispose();
  return 42;
})()`);

  // DisposableStack adopt().
  await fail(`disposable_stack.adopt(42, function(v) {return v})`);
  await success(42, `(() => {
  let stack = new DisposableStack();
  stack.adopt(42, function(v) {return v});
  return 42;
})()`);

  // DisposableStack defer().
  await fail(`disposable_stack.defer(() => console.log(42))`);
  await success(42, `(() => {
  let stack = new DisposableStack();
  stack.defer(() => console.log(42));
  return 42;
})()`);

  // DisposableStack move().
  await fail(`let new_disposable_stack = disposable_stack.move()`);
  await success(42, `(() => {
  let stack = new DisposableStack();
  const disposable = {
    value: 1,
    [Symbol.dispose]() {
      return 43;
    }
  };
  stack.use(disposable);
  let new_disposable_stack = stack.move()
  return 42;
})()`);

  // AsyncDisposableStack use().
  await fail(`(() => {
  const async_disposable = {
    value: 1,
    [Symbol.asyncDispose]() {
      return 43;
    }
  };
  async_disposable_stack.use(async_disposable);
})()`);
  await success(42, `(() => {
  let stack = new AsyncDisposableStack();
  const async_disposable = {
    value: 1,
    [Symbol.asyncDispose]() {
      return 43;
    }
  };
  stack.use(async_disposable);
  return 42;
})()`);

  // AsyncDisposableStack disposeAsync().
  await fail(`async_disposable_stack.disposeAsync()`);
  await success(42, `(() => {
  let stack = new AsyncDisposableStack();
  stack.disposeAsync();
  return 42;
})()`);

  // AsyncDisposableStack adopt().
  await fail(`async_disposable_stack.adopt(42, function(v) {return v})`);
  await success(42, `(() => {
  let stack = new AsyncDisposableStack();
  stack.adopt(42, function(v) {return v});
  return 42;
})()`);

  // AsyncDisposableStack defer().
  await fail(`async_disposable_stack.defer(() => console.log(42))`);
  await success(42, `(() => {
  let stack = new AsyncDisposableStack();
  stack.defer(() => console.log(42));
  return 42;
})()`);

  // AsyncDisposableStack move().
  await fail(`let new_async_Sdisposable_stack = async_disposable_stack.move()`);
  await success(42, `(() => {
  let stack = new AsyncDisposableStack();
  const async_disposable = {
    value: 1,
    [Symbol.asyncDispose]() {
      return 43;
    }
  };
  stack.use(async_disposable);
  let new_async_disposable_stack = stack.move()
  return 42;
})()`);

  InspectorTest.completeTest();
})().catch(e => {
  InspectorTest.log('Uncaught: ' + e);
  InspectorTest.completeTest();
});
