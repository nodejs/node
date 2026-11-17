// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --js-defer-import-eval --bundle --allow-natives-syntax --turbofan

// An exception thrown while a deferred module is evaluated synchronously must
// produce a stack trace that contains the frames of the deferred module body
// followed by the frames of the code whose namespace access triggered the
// evaluation, all with correct source positions.

// JS_BUNDLE_MODULE:get.mjs
function thrower() {
  throw new Error('get');
}
thrower();
export const foo = 42;

// JS_BUNDLE_MODULE:has.mjs
throw new Error('has');
export const foo = 42;

// JS_BUNDLE_MODULE:keys.mjs
throw new Error('keys');
export const foo = 42;

// JS_BUNDLE_MODULE:for-in.mjs
throw new Error('for-in');
export const foo = 42;

// JS_BUNDLE_MODULE:warm.mjs
export const foo = 42;

// JS_BUNDLE_MODULE:optimized.mjs
throw new Error('optimized');
export const foo = 42;

// JS_BUNDLE_MODULE:async.mjs
throw new Error('async');
export const foo = 42;

// JS_BUNDLE_MODULE:structured.mjs
throw new Error('structured');
export const foo = 42;

// JS_BUNDLE_MODULE_ENTRYPOINT:entry.mjs
import defer * as get from 'get.mjs';
import defer * as has from 'has.mjs';
import defer * as keys from 'keys.mjs';
import defer * as forIn from 'for-in.mjs';
import defer * as warm from 'warm.mjs';
import defer * as optimized from 'optimized.mjs';
import defer * as asyncNs from 'async.mjs';
import defer * as structured from 'structured.mjs';

// Returns the frames of `e.stack`, without the directory part of file names.
function stackOf(e) {
  return e.stack.split('\n').slice(1).map(
      line => line.trim().replace(/[^ (]*\//, ''));
}

// [[Get]] from a function, with a function call inside the module body.
function readGet() {
  return get.foo;
}
let getError;
try { readGet(); } catch (e) { getError = e; }
assertEquals([
  'at thrower (get.mjs:2:9)',
  'at get.mjs:4:1',
  'at readGet (entry.mjs:18:14)',
  'at entry.mjs:21:7',
], stackOf(getError));

// A second access rethrows the same error, keeping the original stack.
try { readGet(); } catch (e) { assertSame(getError, e); }
assertEquals([
  'at thrower (get.mjs:2:9)',
  'at get.mjs:4:1',
  'at readGet (entry.mjs:18:14)',
  'at entry.mjs:21:7',
], stackOf(getError));

// [[HasProperty]].
let hasError;
try { 'foo' in has; } catch (e) { hasError = e; }
assertEquals([
  'at has.mjs:1:7',
  'at entry.mjs:40:13',
], stackOf(hasError));

// [[OwnPropertyKeys]], through a builtin.
let keysError;
try { Object.keys(keys); } catch (e) { keysError = e; }
assertEquals([
  'at keys.mjs:1:7',
  'at Object.keys (<anonymous>)',
  'at entry.mjs:48:14',
], stackOf(keysError));

// for-in enumeration.
let forInError;
try { for (const k in forIn) {} } catch (e) { forInError = e; }
assertEquals([
  'at for-in.mjs:1:7',
  'at entry.mjs:57:23',
], stackOf(forInError));

// [[Get]] from optimized code. The load is megamorphic, so the optimized code
// reaches the evaluation through the IC instead of deoptimizing first.
function readOptimized(ns) {
  return ns.foo;
}
%PrepareFunctionForOptimization(readOptimized);
for (let i = 0; i < 3; i++) {
  readOptimized(warm);
  readOptimized({foo: 1});
  readOptimized({a: 1, foo: 1});
  readOptimized({b: 1, foo: 1});
  readOptimized({c: 1, foo: 1});
  readOptimized({d: 1, foo: 1});
}
%OptimizeFunctionOnNextCall(readOptimized);
readOptimized(warm);
assertOptimized(readOptimized);
let optimizedError;
try { readOptimized(optimized); } catch (e) { optimizedError = e; }
assertEquals([
  'at optimized.mjs:1:7',
  'at readOptimized (entry.mjs:66:13)',
  'at entry.mjs:81:7',
], stackOf(optimizedError));
assertOptimized(readOptimized);

// [[Get]] after an await, with async callers.
async function inner() {
  await 0;
  return asyncNs.foo;
}
async function outer() {
  await inner();
}
let asyncError;
try { await outer(); } catch (e) { asyncError = e; }
assertEquals([
  'at async.mjs:1:7',
  'at inner (entry.mjs:92:18)',
  'at async outer (entry.mjs:95:3)',
  'at async entry.mjs:98:7',
], stackOf(asyncError));

// Structured call sites.
Error.prepareStackTrace = (e, frames) => frames;
let structuredError;
try { structured.foo; } catch (e) { structuredError = e; }
// Formatting is lazy, so read the stack before restoring the default.
const frames = structuredError.stack;
Error.prepareStackTrace = undefined;
assertEquals(2, frames.length);
const [moduleFrame, accessFrame] = frames;

assertNull(moduleFrame.getFunctionName());
assertTrue(moduleFrame.getFileName().endsWith('structured.mjs'));
assertEquals(1, moduleFrame.getLineNumber());
assertEquals(7, moduleFrame.getColumnNumber());
assertTrue(moduleFrame.isToplevel());

assertNull(accessFrame.getFunctionName());
assertTrue(accessFrame.getFileName().endsWith('entry.mjs'));
assertEquals(109, accessFrame.getLineNumber());
assertEquals(18, accessFrame.getColumnNumber());
assertTrue(accessFrame.isToplevel());
