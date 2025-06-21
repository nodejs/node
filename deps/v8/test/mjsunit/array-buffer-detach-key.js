// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function TestDetachSucceeds(originalKey, detachKey) {
  const ab = new ArrayBuffer(100);
  %ArrayBufferSetDetachKey(ab, originalKey);
  %ArrayBufferDetach(ab, detachKey);
  assertEquals(0, ab.byteLength);  // Detached.
}

function TestDetachFails(originalKey, detachKey) {
  const ab = new ArrayBuffer(100);
  %ArrayBufferSetDetachKey(ab, originalKey);
  assertThrows(() => { %ArrayBufferDetach(ab, detachKey); }, TypeError);
  assertEquals(100, ab.byteLength);  // Not detached.
}

(function DetachWithKey() {
  TestDetachSucceeds("foo", "foo");
  const part1 = "fo";
  const part2 = "o";
  TestDetachSucceeds("foo", part1 + part2);
  TestDetachSucceeds(1337, 1337);
  TestDetachSucceeds(null, null);
  const obj = {a: 1};
  TestDetachSucceeds(obj, obj);

  TestDetachFails("foo", "bar");
  TestDetachFails("foo", undefined);
  TestDetachFails(1337, 1338);
  TestDetachFails(1337, undefined);
  TestDetachFails(null, 0);
  TestDetachFails(null, undefined);
  TestDetachFails({a: 1}, {a: 1});
})();

(function DetachTwice() {
  const ab = new ArrayBuffer(100);
  %ArrayBufferSetDetachKey(ab, "foo");

  %ArrayBufferDetach(ab, "foo");
  assertEquals(0, ab.byteLength);

  // If the detach key is wrong, an error is thrown even if the buffer was
  // already detached.
  assertThrows(() => { %ArrayBufferDetach(ab, "bar"); });
})();

(function DetachWithoutKey() {
  // This test cannot be implemented via TestDetachFails, since the
  // non-existence of the detach key parameter is not the same thing as it being
  // undefined.
  const ab = new ArrayBuffer(100);
  %ArrayBufferSetDetachKey(ab, "foo");

  assertThrows(() => { %ArrayBufferDetach(ab); }, TypeError);
  assertEquals(100, ab.byteLength);  // Not detached.
})();

(function DetachKeyProvidedEvenThoughWeDontNeedOne() {
  // This test cannot be implemented via TestDetachFails, since the
  // non-existence of the detach key parameter is not the same thing as it being
  // undefined.
  const ab1 = new ArrayBuffer(100);  // No detach key.

  assertThrows(() => { %ArrayBufferDetach(ab1, 'unnecessary'); }, TypeError);
  assertEquals(100, ab1.byteLength);  // Not detached.

  %ArrayBufferDetach(ab1, undefined);  // Passing undefined is fine.
  assertEquals(0, ab1.byteLength);  // Detached.

  const ab2 = new ArrayBuffer(100);  // No detach key.
  %ArrayBufferDetach(ab2);  // Passing no detach key is fine.
  assertEquals(0, ab2.byteLength);  // Detached.
})();
