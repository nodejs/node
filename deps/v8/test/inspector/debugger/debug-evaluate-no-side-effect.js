// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect.');

const source = `
let a = 1;
var object = { property : 2,
               get getter() { return 3; }
             };
var string0 = new String("string");
var string1 = { toString() { return "x"; } };
var print = console.log;
var string2 = { toString() { print("x"); return "x"; } };
var array = [4, 5];
var error = new Error();

function simple_return(x) { return x; }
function set_a() { a = 2; }
function get_a() { return a; }
var bound = get_a.bind(0);

function return_arg0() { return return_arg0.arguments[0]; }
function return_caller_name() { return return_caller_name.caller.name; }

var global_eval = eval;

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

  async function success(expectation, source) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
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
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(
        source + ': ' +
        (result.exceptionDetails ? result.result.className : 'succeeded'));
  }

  // Simple test.
  await success(3, '1 + 2');
  // Dynamic load.
  await success([4, 5], 'array');
  // Context load.
  await success(1, 'a');
  // Global and named property load.
  await success(2, 'object.property');
  // Load via read-only getter.
  await success(3, 'object.getter');
  // Implicit call to read-only toString.
  await success('xy', 'string1 + \'y\'');
  // Keyed property load.
  await success(5, 'array[1]');
  // Call to read-only function.
  await success(1, 'get_a()');
  await success(1, 'bound()');
  await success({}, 'new get_a()');
  // Call to read-only function within try-catch.
  await success(1, 'try { get_a() } catch (e) {}');
  // Call to C++ built-in.
  await success(Math.sin(2), 'Math.sin(2)');
  // Call to allowlisted get accessors.
  await success(3, '\'abc\'.length');
  await success(2, 'array.length');
  await success(1, '\'x\'.length');
  await success(0, 'set_a.length');
  await success('set_a', 'set_a.name');
  await success(0, 'bound.length');
  await success('bound get_a', 'bound.name');
  await success(1, 'return_arg0(1)');
  await success('f', '(function f() { return return_caller_name() })()');
  // Non-evaluated call.
  // Constructed literals.
  await success([1], '[1]');
  await success({x: 1}, '({x: 1})');
  await success([1], '[a]');
  await success({x: 1}, '({x: a})');
  // Test that template literal evaluation fails.
  await fail('simple_return`1`');
  // Test that non-read-only code fails.
  await fail('exception = 1');
  // Test that calling a non-read-only function fails.
  await fail('set_a()');
  await fail('new set_a()');
  // Test that implicit call to a non-read-only function fails.
  await fail('string2 + \'y\'');
  // Test that try-catch does not catch the EvalError.
  await fail('try { set_a() } catch (e) {}');
  // Test that call to set accessor fails.
  await fail('array.length = 4');
  await fail('set_a.name = \'set_b\'');
  await fail('set_a.length = 1');
  await fail('bound.name = \'bound\'');
  await fail('bound.length = 1');
  await fail('set_a.prototype = null');
  // Test that call to non-allowlisted get accessor fails.
  await fail('error.stack');
  // Call to set accessors with receiver check.
  await success(1, '[].length = 1');
  await success(1, '\'x\'.length = 1');
  await fail('string0.length = 1');
  await success(1, '(new String(\'abc\')).length = 1');
  await success('g', '(function(){}).name = \'g\'');
  await success(1, '(function(){}).length = 1');
  await success('g', 'get_a.bind(0).name = \'g\'');
  await success(1, 'get_a.bind(0).length = 1');
  await success(null, '(function(){}).prototype = null');
  await fail('(new Error()).stack.length > 1');
  await fail('(new Error()).stack = \'a\'');
  // Eval is not allowed.
  await fail('eval(\'Math.sin(1)\')');
  await fail('eval(\'exception = 1\')');
  await fail('global_eval(\'1\')');
  await success(1, '(() => { var a = 1; return a++; })()');

  await Protocol.Debugger.resume();
  await evaluatePromise;
  const {result: {result: {value: aValue}}} =
      await Protocol.Runtime.evaluate({expression: 'a'});
  InspectorTest.log('a: ' + aValue);

  InspectorTest.completeTest();
})().catch(e => {
  InspectorTest.log('Uncaught: ' + e);
  InspectorTest.completeTest();
});
