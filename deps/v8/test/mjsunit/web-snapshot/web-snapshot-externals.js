// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-web-snapshots --allow-natives-syntax --verify-heap

const external_1 = {external: 1};
const external_2 = {external: 2};
const object = {
  a: [1,2],
  b: external_1,
  c: [external_1, external_2],
  d: { d_a: external_2 }
};

(function testNoExternals() {
  const snapshot = WebSnapshot.serialize(object);
  const deserialized = WebSnapshot.deserialize(snapshot);
  %HeapObjectVerify(deserialized);
  assertEquals(object, deserialized);
  assertEquals(external_1, deserialized.b);
  assertNotSame(external_1, deserialized.b);
  assertEquals(external_2, deserialized.d.d_a);
  assertNotSame(external_2, deserialized.d.d_a);
})();

(function testOneExternals() {
  const externals = [external_1];
  const snapshot = WebSnapshot.serialize(object, externals);
  const replaced_externals = [{replacement:1}]
  const deserialized = WebSnapshot.deserialize(snapshot, replaced_externals);
  %HeapObjectVerify(deserialized);
  assertEquals(object.a, deserialized.a);
  assertSame(replaced_externals[0], deserialized.b);
  assertArrayEquals([replaced_externals[0], external_2], deserialized.c);
  assertSame(replaced_externals[0], deserialized.c[0]);
  assertNotSame(external_2, deserialized.c[1]);
  assertEquals(external_2, deserialized.d.d_a);
  assertNotSame(external_2, deserialized.d.d_a);
})();

(function testTwoExternals() {
  const externals = [external_1, external_2];
  const snapshot = WebSnapshot.serialize(object, externals);
  const replaced_externals = [{replacement:1}, {replacement:2}]
  const deserialized = WebSnapshot.deserialize(snapshot, replaced_externals);
  %HeapObjectVerify(deserialized);
  assertEquals(object.a, deserialized.a);
  assertSame(deserialized.b, replaced_externals[0]);
  assertArrayEquals(replaced_externals, deserialized.c);
  assertSame(replaced_externals[0], deserialized.c[0]);
  assertSame(replaced_externals[1], deserialized.c[1]);
  assertSame(replaced_externals[1], deserialized.d.d_a);
})();

(function testApiObject() {
  const api_object = new d8.dom.Div();
  const source_1 = [{}, api_object];
  assertThrows(() => WebSnapshot.serialize(source_1));

  let externals = [external_1]
  const source_2 = [{}, external_1, api_object, api_object];
  const snapshot_2 = WebSnapshot.serialize(source_2, externals);
  %HeapObjectVerify(externals);
  // Check that the unhandled api object is added to the externals.
  assertArrayEquals([external_1, api_object], externals);

  assertThrows(() => WebSnapshot.deserialize(snapshot_2));
  assertThrows(() => WebSnapshot.deserialize(snapshot_2, []));
  assertThrows(() => WebSnapshot.deserialize(snapshot_2, [external_1]));

  const result_2 = WebSnapshot.deserialize(snapshot_2, [external_1, api_object]);
  %HeapObjectVerify(externals);
  %HeapObjectVerify(result_2);
  assertArrayEquals(source_2, result_2);
  assertNotSame(source_2[0], result_2[0]);
  assertSame(external_1, result_2[1]);
  assertSame(api_object, result_2[2]);
  assertSame(api_object, result_2[3]);
})();
