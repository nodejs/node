'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Blob, File } = require('node:buffer');

(async () => {
  for (const create of [
    (parts, options) => new Blob(parts, options),
    (parts, options) => new File(parts, 'test', options),
  ]) {
    // Each dictionary member is converted before the next member is read.
    const calls = [];
    const blob = create([], {
      get endings() {
        calls.push('get endings');
        return {
          toString() {
            calls.push('convert endings');
            return 'transparent';
          },
        };
      },
      get type() {
        calls.push('get type');
        return {
          toString() {
            calls.push('convert type');
            return 'TEXT/PLAIN';
          },
        };
      },
    });
    assert.deepStrictEqual(calls, [
      'get endings', 'convert endings', 'get type', 'convert type',
    ]);
    assert.strictEqual(blob.type, 'text/plain');

    assert.throws(() => create([], {
      endings: 'invalid',
      get type() { return common.mustNotCall('type must not be read')(); },
    }), { name: 'TypeError', code: 'ERR_INVALID_ARG_VALUE' });

    // Dictionary conversion finishes before the input bytes are copied.
    const bytes = new Uint8Array([65]);
    const changed = create([bytes], {
      type: {
        toString() {
          bytes[0] = 66;
          return '';
        },
      },
    });
    assert.strictEqual(await changed.text(), 'B');

    const detached = new Uint8Array([65]);
    const empty = create([detached], {
      type: {
        toString() {
          detached.buffer.transfer();
          return '';
        },
      },
    });
    assert.strictEqual(empty.size, 0);

    // BufferSource copying uses internal slots, not ordinary properties or
    // ArrayBuffer.prototype.slice(), which consults Symbol.species.
    for (const makePart of [
      (buffer) => buffer,
      (buffer) => new Uint8Array(buffer, 1, 2),
      (buffer) => new DataView(buffer, 1, 2),
      (buffer) => Buffer.from(buffer, 1, 2),
    ]) {
      const buffer = new Uint8Array([65, 66, 67, 68]).buffer;
      const part = makePart(buffer);
      if (part !== buffer) {
        for (const property of ['buffer', 'byteOffset', 'byteLength']) {
          Object.defineProperty(part, property, {
            get: common.mustNotCall(`${property} must not be read`),
          });
        }
      }
      buffer.slice = common.mustNotCall('slice must not be called');
      buffer.constructor = {
        get [Symbol.species]() { return common.mustNotCall('species must not be read')(); },
      };
      const copied = create([part]);
      assert.strictEqual(await copied.text(), part === buffer ? 'ABCD' : 'BC');
    }

    // Reject both bare shared buffers and views for browser compatibility.
    for (const part of [
      new SharedArrayBuffer(4),
      new Uint8Array(new SharedArrayBuffer(4)),
      new DataView(new SharedArrayBuffer(4)),
    ]) {
      assert.throws(() => create([part]), {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      });
    }
    for (const part of [
      new ArrayBuffer(4, { maxByteLength: 8 }),
      new Uint8Array(new ArrayBuffer(4, { maxByteLength: 8 })),
      new DataView(new ArrayBuffer(4, { maxByteLength: 8 })),
    ]) {
      assert.throws(() => create([part]), TypeError);
    }

    const source = new Blob(['abc']);
    Object.defineProperty(source, 'size', {
      get: common.mustNotCall('size must not be read'),
    });
    assert.strictEqual(await create([source]).text(), 'abc');

    const gets = [];
    const proxy = new Proxy(source, {
      get(target, key, receiver) {
        gets.push(key);
        return Reflect.get(target, key, receiver);
      },
    });
    assert.strictEqual(await create([proxy]).text(), '[object Blob]');
    assert.deepStrictEqual(gets, [Symbol.toPrimitive, 'toString', Symbol.toStringTag]);
    assert.strictEqual(await create([{ __proto__: source }]).text(), '[object Blob]');
  }

  // File arguments are converted from left to right, with the inherited
  // BlobPropertyBag members converted before FilePropertyBag.lastModified.
  const calls = [];
  const file = new File([{
    toString() {
      calls.push('part');
      return 'x';
    },
  }], {
    toString() {
      calls.push('name');
      return 'file\ud800';
    },
  }, {
    get endings() {
      calls.push('get endings');
      return {
        toString() {
          calls.push('convert endings');
          return 'transparent';
        },
      };
    },
    get type() {
      calls.push('get type');
      return {
        toString() {
          calls.push('convert type');
          return 'TEXT/PLAIN';
        },
      };
    },
    get lastModified() {
      calls.push('get lastModified');
      return {
        valueOf() {
          calls.push('convert lastModified');
          return 1.9;
        },
      };
    },
  });
  assert.deepStrictEqual(calls, [
    'part', 'name', 'get endings', 'convert endings', 'get type', 'convert type',
    'get lastModified', 'convert lastModified',
  ]);
  assert.strictEqual(file.name, 'file\ufffd');
  assert.strictEqual(file.type, 'text/plain');
  assert.strictEqual(file.lastModified, 1);

  assert.throws(() => new File(undefined, {
    toString: common.mustNotCall('filename must not be converted'),
  }), TypeError);
  const nameError = new Error('filename conversion');
  assert.throws(() => new File([], {
    toString() { throw nameError; },
  }, {
    get endings() { return common.mustNotCall('options must not be read')(); },
  }), nameError);

  const bytes = new Uint8Array([65]);
  const named = new File([bytes], {
    toString() {
      bytes[0] = 66;
      return 'file';
    },
  });
  assert.strictEqual(await named.text(), 'B');

  const modified = new File([bytes], 'file', {
    lastModified: {
      valueOf() {
        bytes[0] = 67;
        return 1;
      },
    },
  });
  assert.strictEqual(await modified.text(), 'C');

  for (const [lastModified, expected] of [
    [1.9, 1],
    [-1.9, -1],
    [Infinity, 0],
    [-Infinity, 0],
    [NaN, 0],
    [-0, 0],
    [2 ** 63, -(2 ** 63)],
    [2 ** 64, 0],
    [null, 0],
  ]) {
    assert.strictEqual(new File([], '', { lastModified }).lastModified, expected);
  }
})().then(common.mustCall());
