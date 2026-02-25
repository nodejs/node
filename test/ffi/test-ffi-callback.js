// Flags: --experimental-ffi
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { UnsafeCallback, UnsafePointer } = require('node:ffi');

describe('UnsafeCallback constructor', () => {
  it('should throw TypeError when called without arguments', () => {
    assert.throws(() => {
      new UnsafeCallback();
    }, {
      name: 'TypeError',
      message: 'Expected definition and callback arguments',
    });
  });

  it('should throw TypeError when definition is not an object', () => {
    assert.throws(() => {
      new UnsafeCallback('not an object', () => {});
    }, {
      name: 'TypeError',
      message: 'First argument must be a definition object',
    });
  });

  it('should throw TypeError when callback is not a function', () => {
    assert.throws(() => {
      new UnsafeCallback({ parameters: [], result: 'void' }, 'not a function');
    }, {
      name: 'TypeError',
      message: 'Second argument must be a function',
    });
  });

  it('should throw TypeError when definition.parameters is missing', () => {
    assert.throws(() => {
      new UnsafeCallback({ result: 'void' }, () => {});
    }, {
      name: 'TypeError',
      message: 'definition.parameters must be an array',
    });
  });

  it('should throw TypeError when definition.parameters is not an array', () => {
    assert.throws(() => {
      new UnsafeCallback({ parameters: 'not an array', result: 'void' }, () => {});
    }, {
      name: 'TypeError',
      message: 'definition.parameters must be an array',
    });
  });

  it('should throw TypeError when definition.parameters contains non-strings', () => {
    assert.throws(() => {
      new UnsafeCallback({ parameters: ['i32', 42], result: 'void' }, () => {});
    }, {
      name: 'TypeError',
      message: 'definition.parameters must contain only strings',
    });
  });

  it('should throw TypeError when definition.result is missing', () => {
    assert.throws(() => {
      new UnsafeCallback({ parameters: [] }, () => {});
    }, {
      name: 'TypeError',
      message: 'definition.result must be a string',
    });
  });

  it('should throw TypeError when definition.result is not a string', () => {
    assert.throws(() => {
      new UnsafeCallback({ parameters: [], result: 42 }, () => {});
    }, {
      name: 'TypeError',
      message: 'definition.result must be a string',
    });
  });

  it('should create callback with definition, callback, and pointer properties', () => {
    const definition = { parameters: ['i32', 'i32'], result: 'i32' };
    const callbackFn = (a, b) => a + b;
    const callback = new UnsafeCallback(definition, callbackFn);

    assert.ok(callback instanceof UnsafeCallback);
    assert.strictEqual(callback.definition, definition);
    assert.strictEqual(callback.callback, callbackFn);
    assert.strictEqual(typeof callback.pointer, 'object');
    assert.strictEqual(Object.getPrototypeOf(callback.pointer), null);
    assert.strictEqual(typeof UnsafePointer.value(callback.pointer), 'bigint');

    callback.close();
  });
});

describe('UnsafeCallback methods', () => {
  it('should expose close, ref, unref, and threadSafe', () => {
    const callback = new UnsafeCallback(
      { parameters: ['i32', 'i32'], result: 'i32' },
      (a, b) => a + b,
    );

    assert.strictEqual(typeof callback.close, 'function');
    assert.strictEqual(typeof callback.ref, 'function');
    assert.strictEqual(typeof callback.unref, 'function');
    assert.strictEqual(typeof UnsafeCallback.threadSafe, 'function');

    callback.close();
  });

  it('should increment and decrement ref count', () => {
    const callback = new UnsafeCallback(
      { parameters: ['i32'], result: 'i32' },
      (a) => a,
    );

    assert.strictEqual(callback.unref(), 0);
    assert.strictEqual(callback.ref(), 1);
    assert.strictEqual(callback.ref(), 2);
    assert.strictEqual(callback.unref(), 1);
    assert.strictEqual(callback.unref(), 0);

    callback.close();
  });

  it('should create ref-ed callback via threadSafe()', () => {
    const callback = UnsafeCallback.threadSafe(
      { parameters: ['i32'], result: 'i32' },
      (a) => a,
    );

    assert.ok(callback instanceof UnsafeCallback);
    assert.strictEqual(callback.unref(), 0);

    callback.close();
  });

  it('should throw when ref/unref is called after close', () => {
    const callback = new UnsafeCallback(
      { parameters: ['i32'], result: 'i32' },
      (a) => a,
    );

    callback.close();

    assert.throws(() => callback.ref(), {
      name: 'Error',
      message: 'UnsafeCallback is closed',
    });
    assert.throws(() => callback.unref(), {
      name: 'Error',
      message: 'UnsafeCallback is closed',
    });
  });
});
