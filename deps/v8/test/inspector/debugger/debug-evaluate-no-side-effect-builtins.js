// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-staging

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-builtins.');

const source = `
var object = {"foo": "bar"};
var object_with_symbol_key = {[Symbol("a")]: 1};
var object_with_callbacks = { toString: () => "string", valueOf: () => 3};
var symbol_for_a = Symbol.for("a");
var typed_array = new Uint8Array([1, 2, 3]);
var array_buffer = new ArrayBuffer(3);
var data_view = new DataView(new ArrayBuffer(8), 0, 8);
var array = [1,2,3];
var pure_function = function(x) { return x * x; };
var unpure_function = function(x) { array.push(x); };
var stack = new DisposableStack();
var regexp = /\\d/g;
var async_stack = new AsyncDisposableStack();

function getArrayFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(Array.prototype)) {
    if (typeof Array.prototype[f] === "function") {
      result.push(f);
    }
  }
  return result;
}

function getDataViewFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(DataView.prototype)) {
    if (typeof data_view[f] === 'function') {
      result.push(f);
    }
  }
  return result;
}

function getTypedArrayFunctions() {
  const result = [];
  const typed_proto_proto = Object.getPrototypeOf(Object.getPrototypeOf(new Uint8Array()));
  for (const f of Object.getOwnPropertyNames(typed_proto_proto)) {
    if (typeof typed_array[f] === "function" && f !== "constructor") {
      result.push(f);
    }
  }
  return result;
}

function getMathFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(Math)) {
    if (typeof Math[f] === "function") {
      result.push(f);
    }
  }
  return result;
}

function getNumberFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(Number)) {
    if (typeof Number[f] === "function") {
      result.push(f);
    }
  }
  return result;
}

function getNumberPrototypeFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(Number.prototype)) {
    if (typeof Number.prototype[f] === "function") {
      if (f === "toLocaleString" && typeof Intl === "undefined") continue;
      result.push(f);
    }
  }
  return result;
}

function getStringPrototypeFunctions() {
  const result = [];
  for (const f of Object.getOwnPropertyNames(String.prototype)) {
    if (typeof String.prototype[f] === "function") {
      if (f.indexOf("locale") >= 0 || f.indexOf("Locale") >= 0) continue;
      switch (f) {
        case "match":
        case "split":
        case "matchAll":
        case "normalize":
        case "search":
        case "toLowerCase":
        case "toUpperCase":
          continue;
      }
      result.push(f);
    }
  }
  return result;
}

function f() {
  debugger;
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();
  const evaluatePromise = Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const callFrameId = callFrames[0].callFrameId;

  function classOf(object) {
    var string = Object.prototype.toString.call(object);
    return string.substring(8, string.length - 1);
  }

  function ValueOf(value) {
    switch (classOf(value)) {
      case 'Number':
        return Number.prototype.valueOf.call(value);
      case 'BigInt':
        return BigInt.prototype.valueOf.call(value);
      case 'String':
        return String.prototype.valueOf.call(value);
      case 'Boolean':
        return Boolean.prototype.valueOf.call(value);
      case 'Date':
        return Date.prototype.valueOf.call(value);
      default:
        return value;
    }
  }

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

  function deepEquals(a, b) {
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
        return RegExp.prototype.toString.call(a) ===
            RegExp.prototype.toString.call(b);
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
  }

  function assertEquals(expected, found) {
    if (!deepEquals(found, expected)) {
      InspectorTest.log('MISMATCH: expected ' + expected + ', found ' + found);
    }
  }

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

  async function reconstructRemoteObject(obj) {
    let value = obj.value;
    switch (obj.type) {
      case 'object': {
        switch (obj.subtype) {
          case 'array': {
            const array = [];
            const {result: {result: rawProps}} =
                await Protocol.Runtime.getProperties({
                  objectId: obj.objectId,
                  ownProperties: true,
                });
            const props = propertiesToObject(rawProps);
            for (let i = 0; i < props.length; i++) {
              array[i] = props[i];
            }
            value = array;
            break;
          }
          case 'null':
            value = null;
            break;
          default: {
            switch (obj.className) {
              case 'Number':
              case 'String':
              case 'Boolean': {
                const {result: {internalProperties}} =
                    await Protocol.Runtime.getProperties({
                      objectId: obj.objectId,
                      ownProperties: true,
                    });
                for (const p of internalProperties) {
                  if (p.name === '[[PrimitiveValue]]') {
                    value = Object(p.value.value);
                    break;
                  }
                }
                break;
              }
              default: {
                const {result: {result: rawProps}} =
                    await Protocol.Runtime.getProperties({
                      objectId: obj.objectId,
                      ownProperties: true,
                    });
                value = propertiesToObject(rawProps);
                break;
              }
            }
            break;
          }
        }
        break;
      }
      case 'undefined':
        value = undefined;
        break;
      case 'number':
        if (obj.description === 'NaN')
          value = NaN;
        else if (obj.unserializableValue === '-0')
          value = -0;
        else if (obj.unserializableValue === 'Infinity')
          value = Infinity;
        else if (obj.unserializableValue === '-Infinity')
          value = -Infinity;
        break;
      case 'bigint':
        value = eval(obj.unserializableValue);
        break;
      default:
        break;
    }
    return value;
  }

  async function success(expectation, source, logOutput = true) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    if (result.exceptionDetails) {
      InspectorTest.log(source + ' threw: ' + result.result.description);
      return;
    }
    const reconstructed = await reconstructRemoteObject(result.result);
    if (expectation !== undefined) {
      assertEquals(expectation, reconstructed);
      if (logOutput) {
        const actual = result.result.value !== undefined ?
            result.result.value :
            (result.result.unserializableValue || result.result.description);
        InspectorTest.log(source + ': ' + actual);
      }
    }
  }

  async function fail(source) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(
        source + ': ' +
        (result.exceptionDetails ? result.result.className : 'succeeded'));
  }

  // Test some Object functions.
  await success({}, `new Object()`);
  await success({p: 3}, `Object.create({}, { p: { value: 3 } })`);
  await success(
      '[["a",1],["b",2]]', `JSON.stringify(Object.entries({a:1, b:2}))`);
  await success(
      {value: 1, writable: true, enumerable: true, configurable: true},
      `Object.getOwnPropertyDescriptor({a: 1}, "a")`);
  await success(
      '{"a":{"value":1,"writable":true,' +
          '"enumerable":true,"configurable":true}}',
      `JSON.stringify(Object.getOwnPropertyDescriptors({a: 1}))`);
  await success(['a'], `Object.getOwnPropertyNames({a: 1})`);
  await success(
      undefined, `Object.getOwnPropertySymbols(object_with_symbol_key)`);
  await success({}, `Object.getPrototypeOf(Object.create({}))`);
  await success(true, `Object.is(Object, Object)`);
  await success(true, `Object.isExtensible({})`);
  await success(false, `Object.isFrozen({})`);
  await success(false, `Object.isSealed({})`);
  await success([1, 2], `Object.values({a:1, b:2})`);
  await success(['a', 1, 'b', 2], `Object.entries({a:1, b:2}).flat()`);
  await success(['a', 'b'], `Object.keys({a:1, b:2})`);
  await success(
      '{"1":[1],"2":[2]}', `JSON.stringify(Object.groupBy([1,2], x => x))`);

  await fail(`Object.assign({}, {})`);
  await fail(`Object.defineProperties({}, [{p:{value:3}}])`);
  await fail(`Object.defineProperty({}, {p:{value:3}})`);
  await fail(`Object.freeze({})`);
  await fail(`Object.preventExtensions({})`);
  await fail(`Object.seal({})`);
  await fail(`Object.setPrototypeOf({}, {})`);

  // Test some Object.prototype functions.
  await success(true, `({a:1}).hasOwnProperty("a")`);
  await success(true, `Object.prototype.isPrototypeOf({})`);
  await success(true, `({a:1}).propertyIsEnumerable("a")`);
  await success('[object Object]', `({a:1}).toString()`);
  await success('[object Object]', `({a:1}).toLocaleString()`);
  await success('string', `(object_with_callbacks).toString()`);
  await success(3, `(object_with_callbacks).valueOf()`);

  // Test Array functions.
  await success(true, `Array.isArray([1, 2, 3])`);
  await success([], `new Array()`);
  await success([undefined, undefined], `new Array(2)`);
  await success([1, 2], `new Array(1, 2)`);
  await success([1, 2, 3], `Array.from([1, 2, 3])`);
  await success(3, `Array.from([1, 2, 3]).pop()`);
  await success([1, 2, 3], `Array.of(1, 2, 3)`);
  await success(3, `Array.of(1, 2, 3).pop()`);

  const {result: {result: {value: arrayFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getArrayFunctions()',
        returnByValue: true,
      });
  const array_function_param = [
    'flatMap', 'forEach', 'every', 'some', 'reduce', 'reduceRight', 'find',
    'filter', 'map', 'findIndex', 'findLast', 'findLastIndex', 'group',
    'groupToMap'
  ];
  const array_fails = [
    'pop', 'push', 'reverse', 'shift', 'unshift', 'splice', 'sort',
    'copyWithin', 'fill'
  ];
  for (const f of arrayFunctions) {
    if (array_fails.includes(f)) {
      if (array_function_param.includes(f)) {
        await fail(`array.${f}(()=>{});`);
      } else {
        await fail(`array.${f}();`);
      }
    } else if (array_function_param.includes(f)) {
      await success(undefined, `array.${f}(()=>{});`);
    } else {
      await success(undefined, `array.${f}();`);
    }
  }

  // Test ArrayBuffer functions.
  await success(3, `array_buffer.byteLength`);
  await success(2, `array_buffer.slice(1, 3).byteLength`);
  await success(true, `ArrayBuffer.isView(typed_array)`);

  // Test DataView functions.
  await success(undefined, `new DataView(array_buffer, 1, 2)`);
  await success(undefined, `data_view.buffer`);
  await success(undefined, `data_view.byteLength`);
  await success(undefined, `data_view.byteOffset`);
  const {result: {result: {value: dataViewFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getDataViewFunctions()',
        returnByValue: true,
      });
  for (const f of dataViewFunctions) {
    if (f.startsWith('getBig')) {
      await success(0n, `data_view.${f}()`);
    } else if (f.startsWith('get')) {
      await success(0, `data_view.${f}()`);
    }
  }

  // Test TypedArray functions.
  await success({}, `new Uint8Array()`);
  await success({0: 0, 1: 0}, `new Uint8Array(2)`);
  await success({0: 1, 1: 2, 2: 3}, `new Uint8Array(typed_array)`);
  await success(true, `!!typed_array.buffer`);
  await success(0, `typed_array.byteOffset`);
  await success(3, `typed_array.byteLength`);
  await success({0: 1, 1: 2}, `Uint8Array.of(1, 2)`);

  const {result: {result: {value: typedArrayFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getTypedArrayFunctions()',
        returnByValue: true,
      });
  const typed_function_param = [
    'forEach',
    'every',
    'some',
    'reduce',
    'reduceRight',
    'find',
    'filter',
    'map',
    'findIndex',
    'findLast',
    'findLastIndex',
  ];
  const typed_fails = ['reverse', 'sort', 'copyWithin', 'fill', 'set'];
  for (const f of typedArrayFunctions) {
    if (typed_fails.includes(f)) {
      if (typed_function_param.includes(f)) {
        await fail(`typed_array.${f}(()=>{});`);
      } else {
        await fail(`typed_array.${f}();`);
      }
    } else if (typed_function_param.includes(f)) {
      await success(undefined, `typed_array.${f}(()=>{});`);
    } else {
      await success(undefined, `typed_array.${f}();`);
    }
  }

  // Test Math functions.
  const {result: {result: {value: mathFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getMathFunctions()',
        returnByValue: true,
      });
  for (const f of mathFunctions) {
    if (f === 'random') {
      await fail('Math.random();');
    } else if (f === 'sumPrecise') {
      await success(Math[f]([0.5, -0.5]), `Math.${f}([0.5, -0.5]);`, false);
    } else {
      await success(Math[f](0.5, -0.5), `Math.${f}(0.5, -0.5);`, false);
    }
  }

  // Test Number functions.
  await success(new Number(0), `new Number()`);
  const {result: {result: {value: numberFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getNumberFunctions()',
        returnByValue: true,
      });
  for (const f of numberFunctions) {
    await success(Number[f](0.5), `Number.${f}(0.5);`, false);
  }
  const {result: {result: {value: numberProtoFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getNumberPrototypeFunctions()',
        returnByValue: true,
      });
  for (const f of numberProtoFunctions) {
    await success(Number(0.5)[f](5), `Number(0.5).${f}(5);`, false);
  }

  // Test String functions.
  const {result: {result: {value: stringProtoFunctions}}} =
      await Protocol.Runtime.evaluate({
        expression: 'getStringPrototypeFunctions()',
        returnByValue: true,
      });
  for (const f of stringProtoFunctions) {
    await success('abcd'[f](2), `"abcd".${f}(2);`, false);
  }

  await success(new String(), `new String()`);
  await success(' ', 'String.fromCodePoint(0x20)');
  await success(' ', 'String.fromCharCode(0x20)');
  await success('abcd', '\'abCd\'.toLocaleLowerCase()');
  await success('ABCD', '\'abcd\'.toLocaleUpperCase()');
  await success('abcd', '\'abCd\'.toLowerCase()');
  await success('ABCD', '\'abcd\'.toUpperCase()');
  await success('a', '\'abcd\'.match(\'a\')[0]');
  await success('a', '\'abcd\'.match(/a/)[0]');
  await fail('\'1234\'.match(regexp)');
  await success(
      '[object RegExp String Iterator]', '\'abcd\'.matchAll(\'a\').toString()');
  await success(
      '[object RegExp String Iterator]', '\'abcd\'.matchAll(/a/g).toString()');
  await fail('\'1234\'.matchAll(regexp)');
  await success('ebcd', '\'abcd\'.replace(\'a\', \'e\')');
  await success('ebcd', '\'abcd\'.replace(/a/, \'e\')');
  await fail('\'135\'.replace(regexp, \'e\')');
  await success('ebcd', '\'abcd\'.replaceAll(\'a\', \'e\')');
  await success('ebcd', '\'abcd\'.replaceAll(/a/g, \'e\')');
  await fail('\'135\'.replaceAll(regexp, \'e\')');
  await success(1, '\'abcd\'.search(\'b\')');
  await success(1, '\'abcd\'.search(/b/)');
  await fail('\'12a34b\'.search(regexp)');
  await success(['a', 'cd'], '\'abcd\'.split(\'b\')');
  await success(['a', 'cd'], '\'abcd\'.split(/b/)');
  await fail('\'12a34b\'.split(regexp)');
  await success(-1, '\'abcd\'.localeCompare(\'abce\')');
  await success('abcd', '\'abcd\'.normalize(\'NFC\')');

  // Test RegExp functions.
  await fail(`/a/.compile()`);
  await success('a', `/a/.exec('abc')[0]`);
  await success(true, `/a/.test('abc')`);
  await fail(`/a/.toString()`);

  // Test JSON functions.
  await success(
      '{"abc":[1,2]}', 'JSON.stringify(JSON.parse(\'{"abc":[1,2]}\'))');

  // Test Symbol functions.
  await success(undefined, `Symbol("a")`);
  await fail(
      `Symbol.for("a")`);  // Symbol.for can be observed via Symbol.keyFor.
  await success('a', `Symbol.keyFor(symbol_for_a)`);
  await success('Symbol(a)', `symbol_for_a.valueOf().toString()`);
  await success('Symbol(a)', `symbol_for_a[Symbol.toPrimitive]().toString()`);

  // Test Reflect functions.
  await success(4, `Reflect.apply(pure_function, undefined, [2])`);
  await fail(`Reflect.apply(unpure_function, undefined, [2])`);
  await success('foo', `Reflect.construct(String, ["foo"]).toString()`);
  await fail(`Reflect.construct(unpure_function, ["foo"])`);
  await success('bar', `Reflect.getOwnPropertyDescriptor(object, "foo").value`);
  await success(true, `Reflect.getPrototypeOf(object) === Object.prototype`);
  await success(true, `Reflect.has(object, "foo")`);
  await success(true, `Reflect.isExtensible(object)`);
  await success('foo', `Reflect.ownKeys(object)[0]`);
  await fail(`Reflect.defineProperty(object, "baz", {})`);
  await fail(`Reflect.deleteProperty(object, "foo")`);
  await fail(`Reflect.preventExtensions(object)`);
  await fail(`Reflect.set(object, "great", "expectations")`);
  await fail(`Reflect.setPrototypeOf(object, Array.prototype)`);

  // Test some Map functions
  await success('[1]', `JSON.stringify(Map.groupBy([1,2], x => x).get(1))`);

  // Test DisposableStack functions.
  await success({}, `new DisposableStack()`);
  await success(false, `stack.disposed`);

  // Test AsyncDisposableStack functions.
  await success({}, `new AsyncDisposableStack()`);
  await success(false, `async_stack.disposed`);

  await Protocol.Debugger.resume();
  await evaluatePromise;
  InspectorTest.completeTest();
})();
