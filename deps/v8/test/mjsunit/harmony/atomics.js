// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-atomics --harmony-sharedarraybuffer
//

function toRangeWrapped(value) {
  var range = this.max - this.min + 1;
  while (value < this.min) {
    value += range;
  }
  while (value > this.max) {
    value -= range;
  }
  return value;
}

function toRangeClamped(value) {
  if (value < this.min) return this.min;
  if (value > this.max) return this.max;
  return value;
}

function makeConstructorObject(constr, min, max, toRange) {
  var o = {constr: constr, min: min, max: max};
  o.toRange = toRange.bind(o);
  return o;
}

var IntegerTypedArrayConstructors = [
  makeConstructorObject(Int8Array, -128, 127, toRangeWrapped),
  makeConstructorObject(Int16Array, -32768, 32767, toRangeWrapped),
  makeConstructorObject(Int32Array, -0x80000000, 0x7fffffff, toRangeWrapped),
  makeConstructorObject(Uint8Array, 0, 255, toRangeWrapped),
  makeConstructorObject(Uint8ClampedArray, 0, 255, toRangeClamped),
  makeConstructorObject(Uint16Array, 0, 65535, toRangeWrapped),
  makeConstructorObject(Uint32Array, 0, 0xffffffff, toRangeWrapped),
];

var TypedArrayConstructors = IntegerTypedArrayConstructors.concat([
  {constr: Float32Array},
  {constr: Float64Array},
]);

(function TestBadArray() {
  var ab = new ArrayBuffer(16);
  var u32a = new Uint32Array(16);
  var sab = new SharedArrayBuffer(128);
  var sf32a = new Float32Array(sab);
  var sf64a = new Float64Array(sab);

  // Atomic ops required shared typed arrays
  [undefined, 1, 'hi', 3.4, ab, u32a, sab].forEach(function(o) {
    assertThrows(function() { Atomics.compareExchange(o, 0, 0, 0); },
                 TypeError);
    assertThrows(function() { Atomics.load(o, 0); }, TypeError);
    assertThrows(function() { Atomics.store(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.add(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.sub(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.and(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.or(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.xor(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.exchange(o, 0, 0); }, TypeError);
  });

  // Arithmetic atomic ops require integer shared arrays
  [sab, sf32a, sf64a].forEach(function(o) {
    assertThrows(function() { Atomics.add(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.sub(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.and(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.or(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.xor(o, 0, 0); }, TypeError);
    assertThrows(function() { Atomics.exchange(o, 0, 0); }, TypeError);
  });
})();

function testAtomicOp(op, ia, index, expectedIndex, name) {
  for (var i = 0; i < ia.length; ++i)
    ia[i] = 22;

  ia[expectedIndex] = 0;
  assertEquals(0, op(ia, index, 0, 0), name);
  assertEquals(0, ia[expectedIndex], name);

  for (var i = 0; i < ia.length; ++i) {
    if (i == expectedIndex) continue;
    assertEquals(22, ia[i], name);
  }
}

(function TestBadIndex() {
  var sab = new SharedArrayBuffer(8);
  var si32a = new Int32Array(sab);

  // Non-integer indexes are converted to an integer first, so they should all
  // operate on index 0.
  [undefined, null, false, 'hi', {}].forEach(function(i) {
    var name = String(i);

    testAtomicOp(Atomics.compareExchange, si32a, i, 0, name);
    testAtomicOp(Atomics.load, si32a, i, 0, name);
    testAtomicOp(Atomics.store, si32a, i, 0, name);
    testAtomicOp(Atomics.add, si32a, i, 0, name);
    testAtomicOp(Atomics.sub, si32a, i, 0, name);
    testAtomicOp(Atomics.and, si32a, i, 0, name);
    testAtomicOp(Atomics.or, si32a, i, 0, name);
    testAtomicOp(Atomics.xor, si32a, i, 0, name);
    testAtomicOp(Atomics.exchange, si32a, i, 0, name);
  });

  // Out-of-bounds indexes should return undefined.
  // TODO(binji): Should these throw RangeError instead?
  [-1, 2, 100].forEach(function(i) {
    var name = String(i);
    assertEquals(undefined, Atomics.compareExchange(si32a, i, 0, 0), name);
    assertEquals(undefined, Atomics.load(si32a, i), name);
    assertEquals(undefined, Atomics.store(si32a, i, 0), name);
    assertEquals(undefined, Atomics.add(si32a, i, 0), name);
    assertEquals(undefined, Atomics.sub(si32a, i, 0), name);
    assertEquals(undefined, Atomics.and(si32a, i, 0), name);
    assertEquals(undefined, Atomics.or(si32a, i, 0), name);
    assertEquals(undefined, Atomics.xor(si32a, i, 0), name);
    assertEquals(undefined, Atomics.exchange(si32a, i, 0), name);
  });

  // Monkey-patch length and make sure these functions still return undefined.
  Object.defineProperty(si32a, 'length', {get: function() { return 1000; }});
  [2, 100].forEach(function(i) {
    var name = String(i);
    assertEquals(undefined, Atomics.compareExchange(si32a, i, 0, 0), name);
    assertEquals(undefined, Atomics.load(si32a, i), name);
    assertEquals(undefined, Atomics.store(si32a, i, 0), name);
    assertEquals(undefined, Atomics.add(si32a, i, 0), name);
    assertEquals(undefined, Atomics.sub(si32a, i, 0), name);
    assertEquals(undefined, Atomics.and(si32a, i, 0), name);
    assertEquals(undefined, Atomics.or(si32a, i, 0), name);
    assertEquals(undefined, Atomics.xor(si32a, i, 0), name);
    assertEquals(undefined, Atomics.exchange(si32a, i, 0), name);
  });
})();

(function TestGoodIndex() {
  var sab = new SharedArrayBuffer(64);
  var si32a = new Int32Array(sab);

  var valueOf = {valueOf: function(){ return 3;}};
  var toString = {toString: function(){ return '3';}};

  [3, 3.5, '3', '3.5', valueOf, toString].forEach(function(i) {
    var name = String(i);

    testAtomicOp(Atomics.compareExchange, si32a, i, 3, name);
    testAtomicOp(Atomics.load, si32a, i, 3, name);
    testAtomicOp(Atomics.store, si32a, i, 3, name);
    testAtomicOp(Atomics.add, si32a, i, 3, name);
    testAtomicOp(Atomics.sub, si32a, i, 3, name);
    testAtomicOp(Atomics.and, si32a, i, 3, name);
    testAtomicOp(Atomics.or, si32a, i, 3, name);
    testAtomicOp(Atomics.xor, si32a, i, 3, name);
    testAtomicOp(Atomics.exchange, si32a, i, 3, name);
  });
})();

(function TestCompareExchange() {
  TypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      // sta[i] == 0, CAS will store
      assertEquals(0, Atomics.compareExchange(sta, i, 0, 50), name);
      assertEquals(50, sta[i], name);

      // sta[i] == 50, CAS will not store
      assertEquals(50, Atomics.compareExchange(sta, i, 0, 100), name);
      assertEquals(50, sta[i], name);
    }
  });

  // * Exact float values should be OK
  // * Infinity, -Infinity should be OK (has exact representation)
  // * NaN is not OK, it has many representations, cannot ensure successful CAS
  // because it does a bitwise compare
  [1.5, 4.25, -1e8, -Infinity, Infinity].forEach(function(v) {
    var sab = new SharedArrayBuffer(10 * Float32Array.BYTES_PER_ELEMENT);
    var sf32a = new Float32Array(sab);
    sf32a[0] = 0;
    assertEquals(0, Atomics.compareExchange(sf32a, 0, 0, v));
    assertEquals(v, sf32a[0]);
    assertEquals(v, Atomics.compareExchange(sf32a, 0, v, 0));
    assertEquals(0, sf32a[0]);

    var sab2 = new SharedArrayBuffer(10 * Float64Array.BYTES_PER_ELEMENT);
    var sf64a = new Float64Array(sab2);
    sf64a[0] = 0;
    assertEquals(0, Atomics.compareExchange(sf64a, 0, 0, v));
    assertEquals(v, sf64a[0]);
    assertEquals(v, Atomics.compareExchange(sf64a, 0, v, 0));
    assertEquals(0, sf64a[0]);
  });
})();

(function TestLoad() {
  TypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      sta[i] = 0;
      assertEquals(0, Atomics.load(sta, i), name);
      sta[i] = 50;
      assertEquals(50, Atomics.load(sta, i), name);
    }
  });
})();

(function TestStore() {
  TypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      assertEquals(50, Atomics.store(sta, i, 50), name);
      assertEquals(50, sta[i], name);

      assertEquals(100, Atomics.store(sta, i, 100), name);
      assertEquals(100, sta[i], name);
    }
  });

  [1.5, 4.25, -1e8, -Infinity, Infinity, NaN].forEach(function(v) {
    var sab = new SharedArrayBuffer(10 * Float32Array.BYTES_PER_ELEMENT);
    var sf32a = new Float32Array(sab);
    sf32a[0] = 0;
    assertEquals(v, Atomics.store(sf32a, 0, v));
    assertEquals(v, sf32a[0]);

    var sab2 = new SharedArrayBuffer(10 * Float64Array.BYTES_PER_ELEMENT);
    var sf64a = new Float64Array(sab2);
    sf64a[0] = 0;
    assertEquals(v, Atomics.store(sf64a, 0, v));
    assertEquals(v, sf64a[0]);
  });
})();

(function TestAdd() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      assertEquals(0, Atomics.add(sta, i, 50), name);
      assertEquals(50, sta[i], name);

      assertEquals(50, Atomics.add(sta, i, 70), name);
      assertEquals(120, sta[i], name);
    }
  });
})();

(function TestSub() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      sta[i] = 120;
      assertEquals(120, Atomics.sub(sta, i, 50), name);
      assertEquals(70, sta[i], name);

      assertEquals(70, Atomics.sub(sta, i, 70), name);
      assertEquals(0, sta[i], name);
    }
  });
})();

(function TestAnd() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      sta[i] = 0x3f;
      assertEquals(0x3f, Atomics.and(sta, i, 0x30), name);
      assertEquals(0x30, sta[i], name);

      assertEquals(0x30, Atomics.and(sta, i, 0x20), name);
      assertEquals(0x20, sta[i], name);
    }
  });
})();

(function TestOr() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      sta[i] = 0x30;
      assertEquals(0x30, Atomics.or(sta, i, 0x1c), name);
      assertEquals(0x3c, sta[i], name);

      assertEquals(0x3c, Atomics.or(sta, i, 0x09), name);
      assertEquals(0x3d, sta[i], name);
    }
  });
})();

(function TestXor() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      sta[i] = 0x30;
      assertEquals(0x30, Atomics.xor(sta, i, 0x1c), name);
      assertEquals(0x2c, sta[i], name);

      assertEquals(0x2c, Atomics.xor(sta, i, 0x09), name);
      assertEquals(0x25, sta[i], name);
    }
  });
})();

(function TestExchange() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    for (var i = 0; i < 10; ++i) {
      sta[i] = 0x30;
      assertEquals(0x30, Atomics.exchange(sta, i, 0x1c), name);
      assertEquals(0x1c, sta[i], name);

      assertEquals(0x1c, Atomics.exchange(sta, i, 0x09), name);
      assertEquals(0x09, sta[i], name);
    }
  });
})();

(function TestIsLockFree() {
  // For all platforms we support, 1, 2 and 4 bytes should be lock-free.
  assertEquals(true, Atomics.isLockFree(1));
  assertEquals(true, Atomics.isLockFree(2));
  assertEquals(true, Atomics.isLockFree(4));

  // Sizes that aren't equal to a typedarray BYTES_PER_ELEMENT always return
  // false.
  var validSizes = {};
  TypedArrayConstructors.forEach(function(t) {
    validSizes[t.constr.BYTES_PER_ELEMENT] = true;
  });

  for (var i = 0; i < 1000; ++i) {
    if (!validSizes[i]) {
      assertEquals(false, Atomics.isLockFree(i));
    }
  }
})();

(function TestToNumber() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(1 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);

    var valueOf = {valueOf: function(){ return 3;}};
    var toString = {toString: function(){ return '3';}};

    [false, true, undefined, valueOf, toString].forEach(function(v) {
      var name = Object.prototype.toString.call(sta) + ' - ' + v;

      // CompareExchange
      sta[0] = 50;
      assertEquals(50, Atomics.compareExchange(sta, 0, v, v), name);

      // Store
      assertEquals(+v, Atomics.store(sta, 0, v), name);
      assertEquals(v|0, sta[0], name);

      // Add
      sta[0] = 120;
      assertEquals(120, Atomics.add(sta, 0, v), name);
      assertEquals(120 + (v|0), sta[0], name);

      // Sub
      sta[0] = 70;
      assertEquals(70, Atomics.sub(sta, 0, v), name);
      assertEquals(70 - (v|0), sta[0]);

      // And
      sta[0] = 0x20;
      assertEquals(0x20, Atomics.and(sta, 0, v), name);
      assertEquals(0x20 & (v|0), sta[0]);

      // Or
      sta[0] = 0x3d;
      assertEquals(0x3d, Atomics.or(sta, 0, v), name);
      assertEquals(0x3d | (v|0), sta[0]);

      // Xor
      sta[0] = 0x25;
      assertEquals(0x25, Atomics.xor(sta, 0, v), name);
      assertEquals(0x25 ^ (v|0), sta[0]);

      // Exchange
      sta[0] = 0x09;
      assertEquals(0x09, Atomics.exchange(sta, 0, v), name);
      assertEquals(v|0, sta[0]);
    });
  });
})();

(function TestWrapping() {
  IntegerTypedArrayConstructors.forEach(function(t) {
    var sab = new SharedArrayBuffer(10 * t.constr.BYTES_PER_ELEMENT);
    var sta = new t.constr(sab);
    var name = Object.prototype.toString.call(sta);
    var range = t.max - t.min + 1;
    var offset;
    var operand;
    var val, newVal;
    var valWrapped, newValWrapped;

    for (offset = -range; offset <= range; offset += range) {
      // CompareExchange
      sta[0] = val = 0;
      newVal = val + offset + 1;
      newValWrapped = t.toRange(newVal);
      assertEquals(val, Atomics.compareExchange(sta, 0, val, newVal), name);
      assertEquals(newValWrapped, sta[0], name);

      sta[0] = val = t.min;
      newVal = val + offset - 1;
      newValWrapped = t.toRange(newVal);
      assertEquals(val, Atomics.compareExchange(sta, 0, val, newVal), name);
      assertEquals(newValWrapped, sta[0], name);

      // Store
      sta[0] = 0;
      val = t.max + offset + 1;
      valWrapped = t.toRange(val);
      assertEquals(val, Atomics.store(sta, 0, val), name);
      assertEquals(valWrapped, sta[0], name);

      sta[0] = val = t.min + offset - 1;
      valWrapped = t.toRange(val);
      assertEquals(val, Atomics.store(sta, 0, val), name);
      assertEquals(valWrapped, sta[0], name);

      // Add
      sta[0] = val = t.max;
      operand = offset + 1;
      valWrapped = t.toRange(val + operand);
      assertEquals(val, Atomics.add(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      sta[0] = val = t.min;
      operand = offset - 1;
      valWrapped = t.toRange(val + operand);
      assertEquals(val, Atomics.add(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      // Sub
      sta[0] = val = t.max;
      operand = offset - 1;
      valWrapped = t.toRange(val - operand);
      assertEquals(val, Atomics.sub(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      sta[0] = val = t.min;
      operand = offset + 1;
      valWrapped = t.toRange(val - operand);
      assertEquals(val, Atomics.sub(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      // There's no way to wrap results with logical operators, just test that
      // using an out-of-range value is properly wrapped/clamped when written
      // to memory.

      // And
      sta[0] = val = 0xf;
      operand = 0x3 + offset;
      valWrapped = t.toRange(val & operand);
      assertEquals(val, Atomics.and(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      // Or
      sta[0] = val = 0x12;
      operand = 0x22 + offset;
      valWrapped = t.toRange(val | operand);
      assertEquals(val, Atomics.or(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      // Xor
      sta[0] = val = 0x12;
      operand = 0x22 + offset;
      valWrapped = t.toRange(val ^ operand);
      assertEquals(val, Atomics.xor(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);

      // Exchange
      sta[0] = val = 0x12;
      operand = 0x22 + offset;
      valWrapped = t.toRange(operand);
      assertEquals(val, Atomics.exchange(sta, 0, operand), name);
      assertEquals(valWrapped, sta[0], name);
    }

  });
})();
