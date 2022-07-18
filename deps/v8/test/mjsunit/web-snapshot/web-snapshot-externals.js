// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-web-snapshots --allow-natives-syntax

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
  assertEquals(deserialized, object);
  assertEquals(deserialized.b, external_1);
  assertNotSame(deserialized.b, external_1);
  assertEquals(deserialized.d.d_a, external_2);
  assertNotSame(deserialized.d.d_a, external_2);
})();

(function testOneExternals() {
  const externals = [ external_1];
  const snapshot = WebSnapshot.serialize(object, externals);
  const replaced_externals = [{replacement:1}]
  const deserialized = WebSnapshot.deserialize(snapshot, replaced_externals);
  %HeapObjectVerify(deserialized);
  assertEquals(deserialized.a, object.a);
  assertSame(deserialized.b, replaced_externals[0]);
  assertArrayEquals(deserialized.c, [replaced_externals[0], external_2]);
  assertSame(deserialized.c[0], replaced_externals[0]);
  assertNotSame(deserialized.c[1], external_2);
  assertEquals(deserialized.d.d_a, external_2);
  assertNotSame(deserialized.d.d_a, external_2);
})();

(function testTwoExternals() {
  const externals = [external_1, external_2];
  const snapshot = WebSnapshot.serialize(object, externals);
  const replaced_externals = [{replacement:1}, {replacement:2}]
  const deserialized = WebSnapshot.deserialize(snapshot, replaced_externals);
  %HeapObjectVerify(deserialized);
  assertEquals(deserialized.a, object.a);
  assertSame(deserialized.b, replaced_externals[0]);
  assertArrayEquals(deserialized.c, replaced_externals);
  assertSame(deserialized.c[0], replaced_externals[0]);
  assertSame(deserialized.c[1], replaced_externals[1]);
  assertSame(deserialized.d.d_a, replaced_externals[1]);
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
  assertArrayEquals(externals, [external_1, api_object]);

  assertThrows(() => WebSnapshot.deserialize(snapshot_2));
  assertThrows(() => WebSnapshot.deserialize(snapshot_2, []));
  assertThrows(() => WebSnapshot.deserialize(snapshot_2, [external_1]));

  const result_2 = WebSnapshot.deserialize(snapshot_2, [external_1, api_object]);
  %HeapObjectVerify(externals);
  %HeapObjectVerify(result_2);
  assertArrayEquals(result_2, source_2);
  assertNotSame(result_2[0], source_2[0]);
  assertSame(result_2[1], external_1);
  assertSame(result_2[2], api_object);
  assertSame(result_2[3], api_object);
})();
