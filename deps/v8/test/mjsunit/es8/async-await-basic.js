// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Do not install `AsyncFunction` constructor on global object

function assertThrowsAsync(run, errorType, message) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  if (typeof promise !== "object" || typeof promise.then !== "function") {
    throw new MjsUnitAssertionError(
        "Expected " + run.toString() +
        " to return a Promise, but it returned " + PrettyPrint(promise));
  }

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %RunMicrotasks();

  if (!hadError) {
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw " + errorType.name +
        ", but did not throw.");
  }
  if (!(actual instanceof errorType))
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw " + errorType.name +
        ", but threw '" + actual + "'");
  if (message !== void 0 && actual.message !== message)
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw '" + message + "', but threw '" +
        actual.message + "'");
};

function assertEqualsAsync(expected, run, msg) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  if (typeof promise !== "object" || typeof promise.then !== "function") {
    throw new MjsUnitAssertionError(
        "Expected " + run.toString() +
        " to return a Promise, but it returned " + PrettyPrint(promise));
  }

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %RunMicrotasks();

  if (hadError) throw actual;

  assertTrue(
      hadValue, "Expected '" + run.toString() + "' to produce a value");

  assertEquals(expected, actual, msg);
};

assertEquals(undefined, this.AsyncFunction);
let AsyncFunction = (async function() {}).constructor;

// The AsyncFunction Constructor is the %AsyncFunction% intrinsic object and
// is a subclass of Function.
// (https://tc39.github.io/ecmascript-asyncawait/#async-function-constructor)
assertEquals(Object.getPrototypeOf(AsyncFunction), Function);
assertEquals(Object.getPrototypeOf(AsyncFunction.prototype),
             Function.prototype);
assertTrue(async function() {} instanceof Function);


// Let functionPrototype be the intrinsic object %AsyncFunctionPrototype%.
async function asyncFunctionForProto() {}
assertEquals(AsyncFunction.prototype,
             Object.getPrototypeOf(asyncFunctionForProto));
assertEquals(AsyncFunction.prototype,
             Object.getPrototypeOf(async function() {}));
assertEquals(AsyncFunction.prototype, Object.getPrototypeOf(async () => {}));
assertEquals(AsyncFunction.prototype,
             Object.getPrototypeOf({ async method() {} }.method));
assertEquals(AsyncFunction.prototype, Object.getPrototypeOf(AsyncFunction()));
assertEquals(AsyncFunction.prototype,
             Object.getPrototypeOf(new AsyncFunction()));

// AsyncFunctionCreate does not produce an object with a Prototype
assertEquals(undefined, asyncFunctionForProto.prototype);
assertEquals(false, asyncFunctionForProto.hasOwnProperty("prototype"));
assertEquals(undefined, (async function() {}).prototype);
assertEquals(false, (async function() {}).hasOwnProperty("prototype"));
assertEquals(undefined, (async() => {}).prototype);
assertEquals(false, (async() => {}).hasOwnProperty("prototype"));
assertEquals(undefined, ({ async method() {} }).method.prototype);
assertEquals(false, ({ async method() {} }).method.hasOwnProperty("prototype"));
assertEquals(undefined, AsyncFunction().prototype);
assertEquals(false, AsyncFunction().hasOwnProperty("prototype"));
assertEquals(undefined, (new AsyncFunction()).prototype);
assertEquals(false, (new AsyncFunction()).hasOwnProperty("prototype"));

assertEquals(1, async function(a) { await 1; }.length);
assertEquals(2, async function(a, b) { await 1; }.length);
assertEquals(1, async function(a, b = 2) { await 1; }.length);
assertEquals(2, async function(a, b, ...c) { await 1; }.length);

assertEquals(1, (async(a) => await 1).length);
assertEquals(2, (async(a, b) => await 1).length);
assertEquals(1, (async(a, b = 2) => await 1).length);
assertEquals(2, (async(a, b, ...c) => await 1).length);

assertEquals(1, ({ async f(a) { await 1; } }).f.length);
assertEquals(2, ({ async f(a, b) { await 1; } }).f.length);
assertEquals(1, ({ async f(a, b = 2) { await 1; } }).f.length);
assertEquals(2, ({ async f(a, b, ...c) { await 1; } }).f.length);

assertEquals(1, AsyncFunction("a", "await 1").length);
assertEquals(2, AsyncFunction("a", "b", "await 1").length);
assertEquals(1, AsyncFunction("a", "b = 2", "await 1").length);
assertEquals(2, AsyncFunction("a", "b", "...c", "await 1").length);

assertEquals(1, (new AsyncFunction("a", "await 1")).length);
assertEquals(2, (new AsyncFunction("a", "b", "await 1")).length);
assertEquals(1, (new AsyncFunction("a", "b = 2", "await 1")).length);
assertEquals(2, (new AsyncFunction("a", "b", "...c", "await 1")).length);

// AsyncFunction.prototype[ @@toStringTag ]
var descriptor =
    Object.getOwnPropertyDescriptor(AsyncFunction.prototype,
                                    Symbol.toStringTag);
assertEquals("AsyncFunction", descriptor.value);
assertEquals(false, descriptor.enumerable);
assertEquals(false, descriptor.writable);
assertEquals(true, descriptor.configurable);

assertEquals(1, AsyncFunction.length);

// Let F be ! FunctionAllocate(functionPrototype, Strict, "non-constructor")
async function asyncNonConstructorDecl() {}
assertThrows(() => new asyncNonConstructorDecl(), TypeError);
assertThrows(() => asyncNonConstructorDecl.caller, TypeError);
assertThrows(() => asyncNonConstructorDecl.arguments, TypeError);

assertThrows(() => new (async function() {}), TypeError);
assertThrows(() => (async function() {}).caller, TypeError);
assertThrows(() => (async function() {}).arguments, TypeError);

assertThrows(
    () => new ({ async nonConstructor() {} }).nonConstructor(), TypeError);
assertThrows(
    () => ({ async nonConstructor() {} }).nonConstructor.caller, TypeError);
assertThrows(
    () => ({ async nonConstructor() {} }).nonConstructor.arguments, TypeError);

assertThrows(() => new (() => "not a constructor!"), TypeError);
assertThrows(() => (() => 1).caller, TypeError);
assertThrows(() => (() => 1).arguments, TypeError);

assertThrows(() => new (AsyncFunction()), TypeError);
assertThrows(() => AsyncFunction().caller, TypeError);
assertThrows(() => AsyncFunction().arguments, TypeError);

assertThrows(() => new (new AsyncFunction()), TypeError);
assertThrows(() => (new AsyncFunction()).caller, TypeError);
assertThrows(() => (new AsyncFunction()).arguments, TypeError);

// Normal completion
async function asyncDecl() { return "test"; }
assertEqualsAsync("test", asyncDecl);
assertEqualsAsync("test2", async function() { return "test2"; });
assertEqualsAsync("test3", async () => "test3");
assertEqualsAsync("test4", () => ({ async f() { return "test4"; } }).f());
assertEqualsAsync("test5", () => AsyncFunction("no", "return 'test' + no;")(5));
assertEqualsAsync("test6",
                  () => (new AsyncFunction("no", "return 'test' + no;"))(6));

class MyError extends Error {};

// Throw completion
async function asyncDeclThrower(e) { throw new MyError(e); }
assertThrowsAsync(() => asyncDeclThrower("boom!"), MyError, "boom!");
assertThrowsAsync(
  () => (async function(e) { throw new MyError(e); })("boom!!!"),
  MyError, "boom!!!");
assertThrowsAsync(
  () => (async e => { throw new MyError(e) })("boom!!"), MyError, "boom!!");
assertThrowsAsync(
  () => ({ async thrower(e) { throw new MyError(e); } }).thrower("boom!1!"),
  MyError, "boom!1!");
assertThrowsAsync(
  () => AsyncFunction("msg", "throw new MyError(msg)")("boom!2!!"),
  MyError, "boom!2!!");
assertThrowsAsync(
  () => (new AsyncFunction("msg", "throw new MyError(msg)"))("boom!2!!!"),
  MyError, "boom!2!!!");

function resolveLater(value) { return Promise.resolve(value); }
function rejectLater(error) { return Promise.reject(error); }

// Resume after Normal completion
var log = [];
async function resumeAfterNormal(value) {
  log.push("start:" + value);
  value = await resolveLater(value + 1);
  log.push("resume:" + value);
  value = await resolveLater(value + 1);
  log.push("resume:" + value);
  return value + 1;
}

assertEqualsAsync(4, () => resumeAfterNormal(1));
assertEquals("start:1 resume:2 resume:3", log.join(" "));

var O = {
  async resumeAfterNormal(value) {
    log.push("start:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    return value + 1;
  }
};
log = [];
assertEqualsAsync(5, () => O.resumeAfterNormal(2));
assertEquals("start:2 resume:3 resume:4", log.join(" "));

var resumeAfterNormalArrow = async (value) => {
  log.push("start:" + value);
  value = await resolveLater(value + 1);
  log.push("resume:" + value);
  value = await resolveLater(value + 1);
  log.push("resume:" + value);
  return value + 1;
};
log = [];
assertEqualsAsync(6, () => resumeAfterNormalArrow(3));
assertEquals("start:3 resume:4 resume:5", log.join(" "));

var resumeAfterNormalEval = AsyncFunction("value", `
    log.push("start:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    return value + 1;`);
log = [];
assertEqualsAsync(7, () => resumeAfterNormalEval(4));
assertEquals("start:4 resume:5 resume:6", log.join(" "));

var resumeAfterNormalNewEval = new AsyncFunction("value", `
    log.push("start:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    return value + 1;`);
log = [];
assertEqualsAsync(8, () => resumeAfterNormalNewEval(5));
assertEquals("start:5 resume:6 resume:7", log.join(" "));

// Resume after Throw completion
async function resumeAfterThrow(value) {
  log.push("start:" + value);
  try {
    value = await rejectLater("throw1");
  } catch (e) {
    log.push("resume:" + e);
  }
  try {
    value = await rejectLater("throw2");
  } catch (e) {
    log.push("resume:" + e);
  }
  return value + 1;
}

log = [];
assertEqualsAsync(2, () => resumeAfterThrow(1));
assertEquals("start:1 resume:throw1 resume:throw2", log.join(" "));

var O = {
  async resumeAfterThrow(value) {
    log.push("start:" + value);
    try {
      value = await rejectLater("throw1");
    } catch (e) {
      log.push("resume:" + e);
    }
    try {
      value = await rejectLater("throw2");
    } catch (e) {
      log.push("resume:" + e);
    }
    return value + 1;
  }
}
log = [];
assertEqualsAsync(3, () => O.resumeAfterThrow(2));
assertEquals("start:2 resume:throw1 resume:throw2", log.join(" "));

var resumeAfterThrowArrow = async (value) => {
  log.push("start:" + value);
  try {
    value = await rejectLater("throw1");
  } catch (e) {
    log.push("resume:" + e);
  }
  try {
    value = await rejectLater("throw2");
  } catch (e) {
    log.push("resume:" + e);
  }
 return value + 1;
};

log = [];

assertEqualsAsync(4, () => resumeAfterThrowArrow(3));
assertEquals("start:3 resume:throw1 resume:throw2", log.join(" "));

var resumeAfterThrowEval = AsyncFunction("value", `
    log.push("start:" + value);
    try {
      value = await rejectLater("throw1");
    } catch (e) {
      log.push("resume:" + e);
    }
    try {
      value = await rejectLater("throw2");
    } catch (e) {
      log.push("resume:" + e);
    }
    return value + 1;`);
log = [];
assertEqualsAsync(5, () => resumeAfterThrowEval(4));
assertEquals("start:4 resume:throw1 resume:throw2", log.join(" "));

var resumeAfterThrowNewEval = new AsyncFunction("value", `
    log.push("start:" + value);
    try {
      value = await rejectLater("throw1");
    } catch (e) {
      log.push("resume:" + e);
    }
    try {
      value = await rejectLater("throw2");
    } catch (e) {
      log.push("resume:" + e);
    }
    return value + 1;`);
log = [];
assertEqualsAsync(6, () => resumeAfterThrowNewEval(5));
assertEquals("start:5 resume:throw1 resume:throw2", log.join(" "));

async function foo() {}
assertEquals("async function foo() {}", foo.toString());
assertEquals("async function () {}", async function () {}.toString());
assertEquals("async x => x", (async x => x).toString());
assertEquals("async x => { return x }", (async x => { return x }).toString());
class AsyncMethod { async foo() { } }
assertEquals("async foo() { }",
             Function.prototype.toString.call(AsyncMethod.prototype.foo));
assertEquals("async foo() { }",
             Function.prototype.toString.call({async foo() { }}.foo));

// Async functions are not constructible
assertThrows(() => class extends (async function() {}) {}, TypeError);

// Regress v8:5148
assertEqualsAsync("1", () => (async({ a = NaN }) => a)({ a: "1" }));
assertEqualsAsync(
    "10", () => (async(foo, { a = NaN }) => foo + a)("1", { a: "0" }));
assertEqualsAsync("2", () => (async({ a = "2" }) => a)({ a: undefined }));
assertEqualsAsync(
    "20", () => (async(foo, { a = "0" }) => foo + a)("2", { a: undefined }));
assertThrows(() => eval("async({ foo = 1 })"), SyntaxError);
assertThrows(() => eval("async(a, { foo = 1 })"), SyntaxError);

// https://bugs.chromium.org/p/chromium/issues/detail?id=638019
async function gaga() {
  let i = 1;
  while (i-- > 0) { await 42 }
}
assertDoesNotThrow(gaga);


{
  let log = [];
  async function foo() {
    try {
      Promise.resolve().then(() => log.push("a"))
    } finally {
      log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "a", "c"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return Promise.resolve().then(() => log.push("a"))
    } finally {
      log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "a", "c"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return await Promise.resolve().then(() => log.push("a"))
    } finally {
      log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["a", "b", "c"], log);
}


{
  let log = [];
  async function foo() {
    try {
      Promise.resolve().then().then(() => log.push("a"))
    } finally {
      log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "c", "a"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return Promise.resolve().then().then(() => log.push("a"))
    } finally {
      log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "a", "c"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return await Promise.resolve().then().then(() => log.push("a"))
    } finally {
      log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["a", "b", "c"], log);
}


{
  let log = [];
  async function foo() {
    try {
      Promise.resolve().then(() => log.push("a"))
    } finally {
      return log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "a", "c"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return Promise.resolve().then(() => log.push("a"))
    } finally {
      return log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "a", "c"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return await Promise.resolve().then(() => log.push("a"))
    } finally {
      return log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["a", "b", "c"], log);
}


{
  let log = [];
  async function foo() {
    try {
      Promise.resolve().then().then(() => log.push("a"))
    } finally {
      return log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "c", "a"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return Promise.resolve().then().then(() => log.push("a"))
    } finally {
      return log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["b", "c", "a"], log);
}

{
  let log = [];
  async function foo() {
    try {
      return await Promise.resolve().then().then(() => log.push("a"))
    } finally {
      return log.push("b");
    }
  }
  foo().then(() => log.push("c"));
  %RunMicrotasks();
  assertEquals(["a", "b", "c"], log);
}
