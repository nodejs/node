// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function prettyPrinted() {}

function formatFailureText() {
  if (expectedText.length <= 40 && foundText.length <= 40) {
    message += ": expected <" + expectedText + "> found <" + foundText + ">";
    message += ":\nexpected:\n" + expectedText + "\nfound:\n" + foundText;
  }
}

function fail(expectedText, found, name_opt) {
  formatFailureText(expectedText, found, name_opt);
  if (!a[aProps[i]][aProps[i]]) { }
}

function deepEquals(a, b) {
  if (a === 0) return 1 / a === 1 / b;
  if (typeof a !== typeof a) return false;
  if (typeof a !== "object" && typeof a !== "function") return false;
  if (objectClass !== classOf()) return false;
  if (objectClass === "RegExp") { }
}

function assertEquals() {
  if (!deepEquals()) {
    fail(prettyPrinted(), undefined, undefined);
  }
}

({y: {}, x: 0.42});

function gaga() {
  return {gx: bar.arguments[0], hx: baz.arguments[0]};
}

function baz() {
  return gaga();
}

function bar(obj) {
  return baz(obj.y);
}

function foo() {
  bar({y: {}, x: 42});
  try { assertEquals() } catch (e) {}
  try { assertEquals() } catch (e) {}
  assertEquals();
}

%PrepareFunctionForOptimization(prettyPrinted);
%PrepareFunctionForOptimization(formatFailureText);
%PrepareFunctionForOptimization(fail);
%PrepareFunctionForOptimization(deepEquals);
%PrepareFunctionForOptimization(assertEquals);
%PrepareFunctionForOptimization(gaga);
%PrepareFunctionForOptimization(baz);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
try { foo() } catch (e) {}
%OptimizeFunctionOnNextCall(foo);
try { foo() } catch (e) {}
%PrepareFunctionForOptimization(prettyPrinted);
%PrepareFunctionForOptimization(formatFailureText);
%PrepareFunctionForOptimization(fail);
%PrepareFunctionForOptimization(deepEquals);
%PrepareFunctionForOptimization(assertEquals);
%PrepareFunctionForOptimization(gaga);
%PrepareFunctionForOptimization(baz);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
try { foo() } catch (e) {}
