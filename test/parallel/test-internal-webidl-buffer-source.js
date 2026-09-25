// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { test } = require('node:test');
const vm = require('vm');

const { converters } = require('internal/webidl');

const TYPED_ARRAY_CTORS = [
  Uint8Array, Int8Array, Uint8ClampedArray,
  Uint16Array, Int16Array,
  Uint32Array, Int32Array,
  Float16Array, Float32Array, Float64Array,
  BigInt64Array, BigUint64Array,
];

function createGrowableSharedArrayBufferView(Ctor) {
  const buffer = createGrowableSharedArrayBuffer();
  const view = new Ctor(buffer);
  return view;
}

function createGrowableSharedArrayBuffer() {
  const buffer = new SharedArrayBuffer(0, { maxByteLength: 1 });
  assert.strictEqual(buffer.growable, true);
  return buffer;
}

test('BufferSource accepts ArrayBuffer', () => {
  const ab = new ArrayBuffer(8);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('BufferSource accepts all TypedArray kinds', () => {
  for (const Ctor of TYPED_ARRAY_CTORS) {
    const ta = new Ctor(4);
    assert.strictEqual(converters.BufferSource(ta), ta);
  }
});

test('BufferSource accepts Buffer', () => {
  const buf = Buffer.alloc(8);
  assert.strictEqual(converters.BufferSource(buf), buf);
});

test('BufferSource accepts DataView', () => {
  const dv = new DataView(new ArrayBuffer(8));
  assert.strictEqual(converters.BufferSource(dv), dv);
});

test('BufferSource accepts cross-realm buffer sources', () => {
  const context = vm.createContext();

  {
    const ab = vm.runInContext('new ArrayBuffer(0)', context);
    assert.strictEqual(converters.BufferSource(ab), ab);
  }

  {
    const dv = vm.runInContext('new DataView(new ArrayBuffer(0))', context);
    assert.strictEqual(converters.BufferSource(dv), dv);
  }

  for (const Ctor of TYPED_ARRAY_CTORS) {
    const ta = vm.runInContext(
      `new ${Ctor.name}(new ArrayBuffer(0))`,
      context,
    );
    assert.strictEqual(converters.BufferSource(ta), ta);
  }
});

test('BufferSource accepts ArrayBuffer subclass instance', () => {
  class MyAB extends ArrayBuffer {}
  const sub = new MyAB(8);
  assert.strictEqual(converters.BufferSource(sub), sub);
});

test('BufferSource accepts TypedArray with null prototype', () => {
  const ta = new Uint8Array(4);
  Object.setPrototypeOf(ta, null);
  assert.strictEqual(converters.BufferSource(ta), ta);
});

test('BufferSource accepts DataView with null prototype', () => {
  const dv = new DataView(new ArrayBuffer(4));
  Object.setPrototypeOf(dv, null);
  assert.strictEqual(converters.BufferSource(dv), dv);
});

test('BufferSource accepts ArrayBuffer with null prototype', () => {
  const ab = new ArrayBuffer(4);
  Object.setPrototypeOf(ab, null);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('BufferSource rejects SharedArrayBuffer', () => {
  assert.throws(
    () => converters.BufferSource(new SharedArrayBuffer(4)),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource rejects SAB-backed TypedArray', () => {
  const view = new Uint8Array(new SharedArrayBuffer(4));
  assert.throws(
    () => converters.BufferSource(view),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => converters.BufferSource(view, { allowShared: true }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource rejects SAB-backed DataView', () => {
  const dv = new DataView(new SharedArrayBuffer(4));
  assert.throws(
    () => converters.BufferSource(dv),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => converters.BufferSource(dv, { allowShared: true }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource rejects SAB view whose buffer prototype was reassigned', () => {
  const sab = new SharedArrayBuffer(4);
  Object.setPrototypeOf(sab, ArrayBuffer.prototype);
  const view = new Uint8Array(sab);
  assert.throws(
    () => converters.BufferSource(view),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource accepts a detached ArrayBuffer', () => {
  const ab = new ArrayBuffer(8);
  structuredClone(ab, { transfer: [ab] });
  assert.strictEqual(ab.byteLength, 0);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('BufferSource rejects resizable ArrayBuffer by default', () => {
  const ab = new ArrayBuffer(0, { maxByteLength: 1 });
  assert.throws(
    () => converters.BufferSource(ab),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('BufferSource handles resizable-backed views with explicit options', () => {
  for (const Ctor of [DataView, ...TYPED_ARRAY_CTORS]) {
    {
      const view = new Ctor(new ArrayBuffer(0, { maxByteLength: 1 }));
      assert.throws(
        () => converters.BufferSource(view),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );
    }

    {
      const view = new Ctor(new ArrayBuffer(0, { maxByteLength: 1 }));
      assert.throws(
        () => converters.BufferSource(view, { allowResizable: false }),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );
    }

    {
      const view = new Ctor(new ArrayBuffer(0, { maxByteLength: 1 }));
      assert.strictEqual(converters.BufferSource(view, {
        allowResizable: true,
      }), view);
    }
  }
});

test('BufferSource rejects SAB-backed views with explicit options', () => {
  for (const Ctor of [DataView, ...TYPED_ARRAY_CTORS]) {
    const view = createGrowableSharedArrayBufferView(Ctor);
    assert.throws(
      () => converters.BufferSource(view, {
        allowShared: true,
        allowResizable: true,
      }),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  }
});

test('AllowSharedBufferSource accepts ArrayBuffer and SharedArrayBuffer', () => {
  const ab = new ArrayBuffer(8);
  const sab = new SharedArrayBuffer(8);

  assert.strictEqual(converters.AllowSharedBufferSource(ab), ab);
  assert.strictEqual(converters.AllowSharedBufferSource(sab), sab);
});

test('AllowSharedBufferSource accepts cross-realm buffers', () => {
  const context = vm.createContext();
  const ab = vm.runInContext('new ArrayBuffer(0)', context);
  const sab = vm.runInContext('new SharedArrayBuffer(0)', context);

  assert.strictEqual(converters.AllowSharedBufferSource(ab), ab);
  assert.strictEqual(converters.AllowSharedBufferSource(sab), sab);
});

test('AllowSharedBufferSource accepts ArrayBuffer and SharedArrayBuffer views', () => {
  const abView = new Uint8Array(new ArrayBuffer(8));
  const sabView = new Uint8Array(new SharedArrayBuffer(8));
  const abDataView = new DataView(new ArrayBuffer(8));
  const sabDataView = new DataView(new SharedArrayBuffer(8));

  assert.strictEqual(converters.AllowSharedBufferSource(abView), abView);
  assert.strictEqual(converters.AllowSharedBufferSource(sabView), sabView);
  assert.strictEqual(
    converters.AllowSharedBufferSource(abDataView), abDataView);
  assert.strictEqual(
    converters.AllowSharedBufferSource(sabDataView), sabDataView);
});

test('AllowSharedBufferSource handles resizable buffers with explicit options', () => {
  const ab = new ArrayBuffer(0, { maxByteLength: 1 });
  const views = [new Uint8Array(ab), new DataView(ab)];

  assert.throws(
    () => converters.AllowSharedBufferSource(ab),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  for (const view of views) {
    assert.throws(
      () => converters.AllowSharedBufferSource(view),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  }
  assert.strictEqual(converters.AllowSharedBufferSource(ab, {
    allowResizable: true,
  }), ab);
  for (const view of views) {
    assert.strictEqual(converters.AllowSharedBufferSource(view, {
      allowResizable: true,
    }), view);
  }
});

test('AllowSharedBufferSource handles growable shared buffers with explicit ' +
     'options', () => {
  const sab = createGrowableSharedArrayBuffer();
  const views = [new Uint8Array(sab), new DataView(sab)];

  assert.throws(
    () => converters.AllowSharedBufferSource(sab),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  for (const view of views) {
    assert.throws(
      () => converters.AllowSharedBufferSource(view),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  }
  assert.strictEqual(converters.AllowSharedBufferSource(sab, {
    allowResizable: true,
  }), sab);
  for (const view of views) {
    assert.strictEqual(converters.AllowSharedBufferSource(view, {
      allowResizable: true,
    }), view);
  }
});

test('Shared buffer growability checks do not read JavaScript properties', () => {
  for (const [buffer, growable] of [
    [new SharedArrayBuffer(8), false],
    [new SharedArrayBuffer(8, { maxByteLength: 8 }), true],
    [new SharedArrayBuffer(8, { maxByteLength: 16 }), true],
    [vm.runInNewContext('new SharedArrayBuffer(8)'), false],
    [vm.runInNewContext('new SharedArrayBuffer(8, { maxByteLength: 16 })'), true],
  ]) {
    const view = new Uint8Array(buffer);
    const dataView = new DataView(buffer);
    for (const mode of ['shadow', 'getter', 'prototype']) {
      if (mode === 'shadow') {
        Object.defineProperty(buffer, 'growable', {
          value: !growable,
          configurable: true,
        });
      } else if (mode === 'getter') {
        Object.defineProperty(buffer, 'growable', {
          get: common.mustNotCall('Unexpected growable getter'),
          configurable: true,
        });
      } else {
        delete buffer.growable;
        Object.setPrototypeOf(buffer, null);
      }

      for (const value of [buffer, view, dataView]) {
        if (growable) {
          assert.throws(() => converters.AllowSharedBufferSource(value), {
            code: 'ERR_INVALID_ARG_TYPE',
          });
        } else {
          assert.strictEqual(converters.AllowSharedBufferSource(value), value);
        }
        assert.strictEqual(converters.AllowSharedBufferSource(value, {
          allowResizable: true,
        }), value);
      }

      if (growable) {
        assert.throws(() => converters.Uint8Array(view, { allowShared: true }), {
          code: 'ERR_INVALID_ARG_TYPE',
        });
      } else {
        assert.strictEqual(converters.Uint8Array(view, { allowShared: true }), view);
      }
      assert.strictEqual(converters.Uint8Array(view, {
        allowShared: true,
        allowResizable: true,
      }), view);
    }
  }
});

test('Shared WebAssembly buffer growability is checked per buffer', {
  skip: typeof WebAssembly === 'undefined',
}, () => {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true });
  for (const [buffer, growable] of [
    [memory.buffer, false],
    [memory.toResizableBuffer(), true],
    [memory.toFixedLengthBuffer(), false],
  ]) {
    for (const value of [buffer, new Uint8Array(buffer), new DataView(buffer)]) {
      if (growable) {
        assert.throws(() => converters.AllowSharedBufferSource(value), {
          code: 'ERR_INVALID_ARG_TYPE',
        });
      } else {
        assert.strictEqual(converters.AllowSharedBufferSource(value), value);
      }
      assert.strictEqual(converters.AllowSharedBufferSource(value, {
        allowResizable: true,
      }), value);
    }
    const view = new Uint8Array(buffer);
    if (growable) {
      assert.throws(() => converters.Uint8Array(view, { allowShared: true }), {
        code: 'ERR_INVALID_ARG_TYPE',
      });
    } else {
      assert.strictEqual(converters.Uint8Array(view, { allowShared: true }), view);
    }
  }
});

test('BufferSource rejects objects with a forged @@toStringTag', () => {
  const fake = { [Symbol.toStringTag]: 'Uint8Array' };
  assert.throws(
    () => converters.BufferSource(fake),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

for (const value of [null, undefined, 0, 1, 1n, '', 'x', true, Symbol('s'), [],
                     {}, () => {}]) {
  test(`BufferSource rejects ${typeof value} ${String(value)}`, () => {
    assert.throws(
      () => converters.BufferSource(value),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  });

  test(`AllowSharedBufferSource rejects ${typeof value} ${String(value)}`, () => {
    assert.throws(
      () => converters.AllowSharedBufferSource(value),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  });
}
