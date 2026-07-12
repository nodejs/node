// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-immutable-arraybuffer --allow-natives-syntax

function TestImmutableBuffer(createSourceBuffer, description) {
  const ab = createSourceBuffer();
  const immutable = ab.transferToImmutable();

  assertTrue(immutable.immutable);
  assertEquals(10, immutable.byteLength);
  assertFalse(immutable.resizable, "Immutable buffer should not be resizable");
  assertEquals(immutable.byteLength, immutable.maxByteLength, "Immutable buffer maxByteLength should equal byteLength");

  // 1. Test Detach throws
  assertThrows(() => %ArrayBufferDetach(immutable), TypeError);

  // 2. Test transfer throws
  assertThrows(() => immutable.transfer(), TypeError);

  // 3. Test resize throws
  assertThrows(() => immutable.resize(20), TypeError);

  // 4. Test slice works (should return a new mutable buffer by default)
  const sliced = immutable.slice(0, 5);
  assertFalse(sliced.immutable);
  assertEquals(5, sliced.byteLength);

  // 5. Test sliceToImmutable works
  const slicedImmutable = immutable.sliceToImmutable(0, 5);
  assertTrue(slicedImmutable.immutable);
  assertEquals(5, slicedImmutable.byteLength);

  // 6. Test DataView set throws
  const dv = new DataView(immutable);
  assertThrows(() => dv.setUint8(0, 1), TypeError);
  assertThrows(() => dv.setUint8(0, 255), TypeError);
  assertThrows(() => dv.setInt8(0, -1), TypeError);
  assertThrows(() => dv.setUint16(0, 0x1234), TypeError);
  assertThrows(() => dv.setFloat64(0, 3.14), TypeError);
  // 7. Test TypedArray set throws
  const ta = new Uint8Array(immutable);

  // Sloppy mode assignment - silent failure
  ta[0] = 1;
  ta[0] = 1;
  assertEquals(0, ta[0]); // Value should not change

  // Strict mode assignment - throws TypeError
  assertThrows(() => {
    "use strict";
    ta[0] = 1;
  }, TypeError);

  assertThrows(() => ta.set([1]), TypeError);
  assertThrows(() => ta.fill(1), TypeError);

  // 8. Test TypedArray.from/of with immutable buffer not directly applicable unless we construct one manually
  // but we can test constructing a TypedArray from immutable buffer (Read-only) matches values
  assertEquals(0, ta[0]);

  // 9. Test property descriptors of TypedArray backed by immutable buffer
  const desc = Object.getOwnPropertyDescriptor(ta, 0);
  assertFalse(desc.writable, "writable should be false");
  assertFalse(desc.configurable, "configurable should be false");
  assertTrue(desc.enumerable, "enumerable should be true");
}

TestImmutableBuffer(() => new ArrayBuffer(10), "Fixed-length ArrayBuffer");
TestImmutableBuffer(() => new ArrayBuffer(10, {maxByteLength: 20}), "Resizable ArrayBuffer");

// Test SharedArrayBuffer cannot be transferred to immutable
(function testSharedArrayBufferTransferToImmutable() {
  const sab = new SharedArrayBuffer(10);
  if (typeof sab.transferToImmutable === 'function') {
      assertThrows(() => sab.transferToImmutable(), TypeError);
  } else {
     assertThrows(() => ArrayBuffer.prototype.transferToImmutable.call(sab), TypeError);
  }
})();

// Test sliceToImmutable on mutable buffers (fixed-length and resizable)
(function testSliceToImmutableOnMutable() {
  // 1. Fixed-length ArrayBuffer
  const ab = new ArrayBuffer(10);
  const immutableSlice = ab.sliceToImmutable(0, 5);
  assertTrue(immutableSlice.immutable);
  assertEquals(5, immutableSlice.byteLength);
  assertFalse(immutableSlice.resizable);
  assertEquals(5, immutableSlice.maxByteLength);

  // 2. Resizable ArrayBuffer
  const rab = new ArrayBuffer(10, {maxByteLength: 20});
  const immutableSliceFromRab = rab.sliceToImmutable(0, 5);
  assertTrue(immutableSliceFromRab.immutable);
  assertEquals(5, immutableSliceFromRab.byteLength);
  assertFalse(immutableSliceFromRab.resizable);
  assertEquals(5, immutableSliceFromRab.maxByteLength);
})();

// Test TypedArray.prototype.map throws TypeError if species creates immutable buffer
(function testTypedArrayMapSpeciesCreateImmutable() {
  const ab = new ArrayBuffer(10);
  const immutable = ab.transferToImmutable();

  class ImmutableTypedArray extends Uint8Array {
    constructor(length) {
      super(immutable, 0, length);
    }
  }

  const ta = new Uint8Array(10);
  // Override the constructor property on the instance to point to an object
  // that specifies our custom species.
  Object.defineProperty(ta, 'constructor', {
    value: {
      [Symbol.species]: ImmutableTypedArray
    },
    configurable: true
  });

  assertThrows(() => ta.map(x => x), TypeError);
})();

// Test sliceToImmutable detachment during argument conversion
(function testSliceToImmutableDetachSideEffects() {
  const ab = new ArrayBuffer(10);
  const detach = {
    valueOf: function() {
      %ArrayBufferDetach(ab);
      return 0;
    }
  };

  assertThrows(() => ab.sliceToImmutable(detach, 5), TypeError);
})();

// Test sliceToImmutable resize during argument conversion
(function testSliceToImmutableResizeSideEffects() {
  const rab = new ArrayBuffer(10, {maxByteLength: 20});
  const resize = {
    valueOf: function() {
      rab.resize(2); // Resize to be smaller than the requested end
      return 0;
    }
  };

  assertThrows(() => rab.sliceToImmutable(resize, 5), RangeError);
})();

// Test sliceToImmutable order of checks
(function testSliceToImmutableDetachAndResizeOrder() {
  const rab = new ArrayBuffer(10, {maxByteLength: 20});
  const resize = {
    valueOf: function() {
      rab.resize(2);   // Resize to be smaller than the requested end
      rab.transfer(); // Also detach it
      return 0;
    }
  };

  assertThrows(() => rab.sliceToImmutable(resize, 5), TypeError);
})();

function createImmutableTypedArray(length = 10, type = Uint8Array) {
  const ab = new ArrayBuffer(length * type.BYTES_PER_ELEMENT);
  const imm = ab.transferToImmutable();
  return new type(imm);
}

// 1. Test TypedArray.prototype.copyWithin
(function testCopyWithin() {
  const ta = createImmutableTypedArray();
  assertThrows(() => ta.copyWithin(0, 1, 2), TypeError);
})();

// 2. Test TypedArray.prototype.reverse
(function testReverse() {
  const ta = createImmutableTypedArray();
  assertThrows(() => ta.reverse(), TypeError);
})();

// 3. Test TypedArray.prototype.sort
(function testSort() {
  const ta = createImmutableTypedArray();
  assertThrows(() => ta.sort(), TypeError);
})();

// 4. Test Atomics
(function testAtomics() {
  // Atomics operations on an Integer TypedArray backed by an *immutable* ArrayBuffer should throw.
  const ta = createImmutableTypedArray(10, Int32Array);

  assertThrows(() => Atomics.store(ta, 0, 1), TypeError);
  assertThrows(() => Atomics.add(ta, 0, 1), TypeError);
  assertThrows(() => Atomics.compareExchange(ta, 0, 0, 1), TypeError);

  // Atomics.load should succeed
  const ab2 = new ArrayBuffer(4);
  new Int32Array(ab2)[0] = 42;
  const imm2 = ab2.transferToImmutable();
  const ta2 = new Int32Array(imm2);

  assertEquals(42, Atomics.load(ta2, 0));
})();

// 5. Test Object.defineProperty / Reflect.defineProperty
(function testDefineProperty() {
  const ta = createImmutableTypedArray();

  // Attempt to redefine an index
  assertThrows(() => Object.defineProperty(ta, "0", { value: 100 }), TypeError);

  // Attempt with Reflect.defineProperty - should return false
  const success = Reflect.defineProperty(ta, "0", { value: 100 });
  assertFalse(success);
})();

// 6. Test Array/TypedArray join/toString/toLocaleString (Read-only operations)
(function testReadOperations() {
  const ab = new ArrayBuffer(2);
  const u8 = new Uint8Array(ab);
  u8[0] = 1; u8[1] = 2;
  const imm = ab.transferToImmutable();
  const ta = new Uint8Array(imm);

  assertEquals("1,2", ta.join());
  assertEquals("1,2", ta.toString());
  // toLocaleString output depends on locale, but should not throw
  assertDoesNotThrow(() => ta.toLocaleString());

  assertEquals(1, ta[0]);
  assertEquals(2, ta[1]);
})();

// 7. Test transferToImmutable on already immutable buffer
(function testTransferToImmutableOnImmutable() {
  const ta = createImmutableTypedArray();
  assertThrows(() => ta.buffer.transferToImmutable(), TypeError);
})();

const taClasses = [
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  BigInt64Array,
  BigUint64Array
];

function CreateImmutableTypedArrayAndInit(ctor, length = 8) {
  const ab = new ArrayBuffer(length * ctor.BYTES_PER_ELEMENT);
  const ta = new ctor(ab);
  for (let i = 0; i < length; i++) {
    ta[i] = (ctor === BigInt64Array || ctor === BigUint64Array) ? BigInt(i) : i;
  }
  const immutable = ab.transferToImmutable();
  return new ctor(immutable);
}

for (const ctor of taClasses) {
  const ta = CreateImmutableTypedArrayAndInit(ctor);
  const len = ta.length;

  // 1. Test [[GetOwnProperty]]
  for (let i = 0; i < len; i++) {
    const desc = Object.getOwnPropertyDescriptor(ta, i);
    assertEquals(ta[i], desc.value);
    assertFalse(desc.writable, `Index ${i} should not be writable`);
    assertFalse(desc.configurable, `Index ${i} should not be configurable`);
    assertTrue(desc.enumerable, `Index ${i} should be enumerable`);
  }

  // 2. Test [[Set]]
  const oldVal = ta[0];
  const newVal = (typeof oldVal === 'bigint') ? 100n : 100;

  // Check strict mode throws
  assertThrows(() => {
    "use strict";
    ta[0] = newVal;
  }, TypeError);
  assertEquals(oldVal, ta[0]);

  // Check non-strict mode (should fail silently)
  ta[0] = newVal;
  assertEquals(oldVal, ta[0]);

  // Case: Same value (Strict Equality)
  const val0 = ta[0];
  Object.defineProperty(ta, "0", { value: val0 }); // Should not throw

  // Explicitly providing matching attributes
  Object.defineProperty(ta, "0", {
    value: val0,
    writable: false,
    configurable: false,
    enumerable: true
  });

  // Case: Different value
  assertThrows(() => {
    Object.defineProperty(ta, "0", { value: newVal });
  }, TypeError);

  // Case: Same value after cast, but not SameValue (only relevant for numbers, not BigInt)
  if (ctor !== BigInt64Array && ctor !== BigUint64Array) {
     if (ta[0] === 0) {
       assertThrows(() => {
         Object.defineProperty(ta, "0", { value: -0 });
       }, TypeError);
     }
  }

  // Case: Change writable to true
  assertThrows(() => {
    Object.defineProperty(ta, "0", { writable: true });
  }, TypeError);

  // Case: Change configurable to true
  assertThrows(() => {
    Object.defineProperty(ta, "0", { configurable: true });
  }, TypeError);

  // Case: Change enumerable to false
  assertThrows(() => {
    Object.defineProperty(ta, "0", { enumerable: false });
  }, TypeError);

  // Case: Accessor descriptor
  assertThrows(() => {
    Object.defineProperty(ta, "0", { get: () => 1 });
  }, TypeError);

  // 4. Test out of bounds
  const outOfBounds = len + 10;
  assertThrows(() => {
    Object.defineProperty(ta, outOfBounds, { value: 1 });
  }, TypeError);

  // 5. Test named properties
  const propName = "prop" + ctor.name;
  ta[propName] = "test";
  assertEquals("test", ta[propName]);

  Object.defineProperty(ta, "otherProp", { value: 123, writable: true, configurable: true, enumerable: true });
  assertEquals(123, ta.otherProp);
}

// Test comprehensive list of TypedArray methods on immutable-backed TA
(function testTypedArrayMethods() {
  const ab = new ArrayBuffer(10);
  const imm = ab.transferToImmutable();
  const ta = new Uint8Array(imm);

  const methods = {
    // Read-only methods (should work)
    at: () => ta.at(0),
    entries: () => ta.entries().next(),
    every: () => ta.every(x => x === 0),
    filter: () => ta.filter(x => true),
    find: () => ta.find(x => x === 0),
    findIndex: () => ta.findIndex(x => x === 0),
    findLast: () => ta.findLast(x => x === 0),
    findLastIndex: () => ta.findLastIndex(x => x === 0),
    forEach: () => ta.forEach(x => {}),
    includes: () => ta.includes(0),
    indexOf: () => ta.indexOf(0),
    join: () => ta.join(),
    keys: () => ta.keys().next(),
    lastIndexOf: () => ta.lastIndexOf(0),
    map: () => ta.map(x => x),
    reduce: () => ta.reduce((a, b) => a + b, 0),
    reduceRight: () => ta.reduceRight((a, b) => a + b, 0),
    slice: () => ta.slice(0),
    some: () => ta.some(x => x === 0),
    subarray: () => ta.subarray(0),
    toLocaleString: () => ta.toLocaleString(),
    toReversed: () => ta.toReversed(),
    toSorted: () => ta.toSorted(),
    toString: () => ta.toString(),
    values: () => ta.values().next(),
    with: () => ta.with(0, 1),

    // Mutating methods (should throw TypeError)
    copyWithin: () => ta.copyWithin(0, 1),
    fill: () => ta.fill(1),
    reverse: () => ta.reverse(),
    set: () => ta.set([1]),
    sort: () => ta.sort(),
  };

  const mutating = new Set(['copyWithin', 'fill', 'reverse', 'set', 'sort']);

  for (const [method, call] of Object.entries(methods)) {
    if (mutating.has(method)) {
      assertThrows(call, TypeError);
    } else {
      assertDoesNotThrow(call);
    }
  }
})();

(function testTransferToImmutableNonGeneric() {
  assertThrows(
      () => ArrayBuffer.prototype.transferToImmutable.call({}),
      TypeError);

  assertThrows(
      () => ArrayBuffer.prototype.transferToImmutable.call(
          new SharedArrayBuffer(10)),
      TypeError);
})();

if (this.Worker) {
  var workerScript = `onmessage = function() {};`;
  var w = new Worker(workerScript, {type: 'string'});

  const ab = new ArrayBuffer(10);
  const immutable = ab.transferToImmutable();

  w.postMessage(immutable);
  assertThrows(() => w.postMessage(immutable, [immutable]), Error);

  w.terminate();
}

(function TestImmutableArrayBufferStructuredClone() {
  const ab = new ArrayBuffer(10);
  const u8 = new Uint8Array(ab);
  for (let i = 0; i < 10; i++) {
    u8[i] = i;
  }

  const immutable = ab.transferToImmutable();
  assertTrue(immutable.immutable);
  assertFalse(immutable.resizable);

  const serialized = d8.serializer.serialize(immutable);
  const deserialized = d8.serializer.deserialize(serialized);

  assertTrue(deserialized.immutable);
  assertEquals(immutable.byteLength, deserialized.byteLength);
  assertFalse(deserialized.resizable);

  const deserializedU8 = new Uint8Array(deserialized);
  assertEquals(10, deserializedU8.length);
  for (let i = 0; i < 10; i++) {
    assertEquals(i, deserializedU8[i]);
  }

  // Ensure the deserialized buffer is really immutable
  assertThrows(() => deserialized.resize(20), TypeError);
  assertThrows(() => deserialized.transfer(), TypeError);
  assertThrows(() => deserialized.transferToFixedLength(), TypeError);
})();

(function TestImmutableArrayBufferInObject() {
  const ab = new ArrayBuffer(10);
  const immutable = ab.transferToImmutable();
  const obj = { buf: immutable };

  const serializedObj = d8.serializer.serialize(obj);
  const deserializedObj = d8.serializer.deserialize(serializedObj);

  assertTrue(deserializedObj.buf.immutable);
  assertEquals(immutable.byteLength, deserializedObj.buf.byteLength);
})();

(function TestSliceToImmutableStructuredClone() {
  const ab = new ArrayBuffer(10);
  const u8 = new Uint8Array(ab);
  for (let i = 0; i < 10; i++) {
    u8[i] = i + 10;
  }

  const slicedImmutable = ab.sliceToImmutable(2, 7); // Length 5
  assertTrue(slicedImmutable.immutable);
  assertEquals(5, slicedImmutable.byteLength);

  const serializedSliced = d8.serializer.serialize(slicedImmutable);
  const deserializedSliced = d8.serializer.deserialize(serializedSliced);

  assertTrue(deserializedSliced.immutable);
  assertEquals(5, deserializedSliced.byteLength);

  const deserializedU8 = new Uint8Array(deserializedSliced);
  for (let i = 0; i < 5; i++) {
    assertEquals(i + 12, deserializedU8[i]);
  }
})();

(function TestEmptyImmutableArrayBuffer() {
  const ab = new ArrayBuffer(0);
  const immutable = ab.transferToImmutable();
  assertTrue(immutable.immutable);
  assertEquals(0, immutable.byteLength);

  const serialized = d8.serializer.serialize(immutable);
  const deserialized = d8.serializer.deserialize(serialized);

  assertTrue(deserialized.immutable);
  assertEquals(0, deserialized.byteLength);
})();

if (this.Worker) {
  var workerScript = `
    onmessage = function({data: ab}) {
      if (!(ab instanceof ArrayBuffer)) {
        postMessage('Error: expected ArrayBuffer');
        return;
      }
      if (!ab.immutable) {
        postMessage('Error: ArrayBuffer should be immutable');
        return;
      }
      if (ab.byteLength !== 10) {
        postMessage('Error: ArrayBuffer byteLength should be 10');
        return;
      }
      const u8 = new Uint8Array(ab);
      for (let i = 0; i < 10; i++) {
        if (u8[i] !== i) {
          postMessage('Error: content mismatch at index ' + i);
          return;
        }
      }
      try {
        ab.resize(20);
        postMessage('Error: resize should fail');
        return;
      } catch (e) {
        if (!(e instanceof TypeError)) {
          postMessage('Error: resize should throw TypeError');
          return;
        }
      }
      postMessage('OK');
    };
  `;

  var w = new Worker(workerScript, {type: 'string'});

  const ab = new ArrayBuffer(10);
  const u8 = new Uint8Array(ab);
  for (let i = 0; i < 10; i++) {
    u8[i] = i;
  }

  const immutable = ab.transferToImmutable();
  assertTrue(immutable.immutable);
  assertFalse(immutable.resizable);

  w.postMessage(immutable);

  assertFalse(immutable.detached);

  assertEquals('OK', w.getMessage());
  w.terminate();
}
