// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestReceiverNonString() {
  const iter = 'a'.matchAll(/./g);
  assertThrows(
    () => iter.next.call(0),
    TypeError
  );
})();


(function TestAncestry() {
  const iterProto = Object.getPrototypeOf('a'.matchAll(/./g));
  const arrProto = Object.getPrototypeOf([][Symbol.iterator]());

  assertSame(Object.getPrototypeOf(iterProto), Object.getPrototypeOf(arrProto));
})();


function TestNoMatch(string, regex_or_string) {
  const next_result = string.matchAll(regex_or_string).next();
  assertSame(undefined, next_result.value);
  assertTrue(next_result.done);
}
TestNoMatch('a', /b/g);
TestNoMatch('a', 'b');


(function NonGlobalRegex() {
  assertThrows(
      () => { const iter = 'ab'.matchAll(/./); },
      TypeError);
})();


function TestGlobalRegex(regex_or_string) {
  const iter = 'ab'.matchAll(/./g);
  let next_result = iter.next();
  assertEquals(['a'], next_result.value);
  assertFalse(next_result.done);

  next_result = iter.next();
  assertEquals(['b'], next_result.value);
  assertFalse(next_result.done);

  next_result = iter.next();
  assertSame(undefined, next_result.value);
  assertTrue(next_result.done);
}
TestGlobalRegex(/./g);
TestGlobalRegex('.');


function TestEmptyGlobalRegExp(regex_or_string) {
  const iter = 'd'.matchAll(regex_or_string);
  let next_result = iter.next();
  assertEquals([''], next_result.value);
  assertFalse(next_result.done);

  next_result = iter.next();
  assertEquals([''], next_result.value);
  assertFalse(next_result.done);

  next_result = iter.next();
  assertSame(undefined, next_result.value);
  assertTrue(next_result.done);
}
TestEmptyGlobalRegExp(undefined);
TestEmptyGlobalRegExp(/(?:)/g);
TestEmptyGlobalRegExp('');


(function TestGlobalRegExpLastIndex() {
  const regex = /./g;
  const string = 'abc';
  regex.exec(string);
  assertSame(1, regex.lastIndex);

  const iter = string.matchAll(regex);

  // Verify an internal RegExp is created and mutations to the provided RegExp
  // are not abservered.
  regex.exec(string);
  assertSame(2, regex.lastIndex);

  let next_result = iter.next();
  assertEquals(['b'], next_result.value);
  assertFalse(next_result.done);
  assertSame(2, regex.lastIndex);
})();
