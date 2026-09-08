// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Atomics.notify with resizable SharedArrayBuffer: length retrieved
// before index coercion.
{
  const gsab = new SharedArrayBuffer(0, {maxByteLength: 4});
  const ta = new Int32Array(gsab);

  let countCalled = false;
  const index = {
    valueOf() {
      gsab.grow(4);
      return 0;
    }
  };
  const count = {
    valueOf() {
      countCalled = true;
      return 1;
    }
  };

  assertThrows(() => Atomics.notify(ta, index, count), RangeError);
  assertEquals(false, countCalled);
  assertEquals(4, gsab.byteLength);
}

// Test Atomics.notify with non-shared resizable ArrayBuffer: length retrieved
// before index coercion.
{
  const rab = new ArrayBuffer(0, {maxByteLength: 4});
  const ta = new Int32Array(rab);

  let countCalled = false;
  const index = {
    valueOf() {
      rab.resize(4);
      return 0;
    }
  };
  const count = {
    valueOf() {
      countCalled = true;
      return 1;
    }
  };

  assertThrows(() => Atomics.notify(ta, index, count), RangeError);
  assertEquals(false, countCalled);
  assertEquals(4, rab.byteLength);
}

// Test Atomics.notify with non-shared detached ArrayBuffer: length retrieved
// before index coercion.
{
  const ab = new ArrayBuffer(4);
  const ta = new Int32Array(ab);

  const index = {
    valueOf() {
      %ArrayBufferDetach(ab);
      return 0;
    }
  };

  assertEquals(0, Atomics.notify(ta, index));
  assertEquals(0, ab.byteLength);
}

// Test Atomics.notify with non-shared resized-to-zero ArrayBuffer: length
// retrieved before index coercion.
{
  const rab = new ArrayBuffer(4, {maxByteLength: 4});
  const ta = new Int32Array(rab);

  const index = {
    valueOf() {
      rab.resize(0);
      return 0;
    }
  };

  assertEquals(0, Atomics.notify(ta, index));
  assertEquals(0, rab.byteLength);
}

// Test Atomics.wait with resizable SharedArrayBuffer: length retrieved before
// index coercion.
if (typeof Atomics.wait === 'function') {
  const gsab = new SharedArrayBuffer(0, {maxByteLength: 4});
  const ta = new Int32Array(gsab);

  let valueCalled = false;
  const index = {
    valueOf() {
      gsab.grow(4);
      return 0;
    }
  };
  const value = {
    valueOf() {
      valueCalled = true;
      return 0;
    }
  };

  assertThrows(() => Atomics.wait(ta, index, value, 0), RangeError);
  assertEquals(false, valueCalled);
  assertEquals(4, gsab.byteLength);
}

// Test Atomics.waitAsync with resizable SharedArrayBuffer: length retrieved
// before index coercion.
if (typeof Atomics.waitAsync === 'function') {
  const gsab = new SharedArrayBuffer(0, {maxByteLength: 4});
  const ta = new Int32Array(gsab);

  let valueCalled = false;
  const index = {
    valueOf() {
      gsab.grow(4);
      return 0;
    }
  };
  const value = {
    valueOf() {
      valueCalled = true;
      return 0;
    }
  };

  assertThrows(() => Atomics.waitAsync(ta, index, value, 0), RangeError);
  assertEquals(false, valueCalled);
  assertEquals(4, gsab.byteLength);
}
