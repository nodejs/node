// Flags: --experimental-ffi
'use strict';
const { skipIfFFIMissing } = require('../common');

skipIfFFIMissing();

const assert = require('node:assert');
const { describe, it } = require('node:test');
const { UnsafePointer, UnsafePointerView } = require('node:ffi');

describe('UnsafePointerView constructor', () => {
  it('should create a valid UnsafePointerView instance', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.strictEqual(typeof view, 'object');
    assert.ok(view instanceof UnsafePointerView);
  });

  it('should throw TypeError when called without arguments', () => {
    assert.throws(() => {
      new UnsafePointerView();
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed a string', () => {
    assert.throws(() => {
      new UnsafePointerView('not a pointer');
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed a number', () => {
    assert.throws(() => {
      new UnsafePointerView(123);
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });
});

describe('UnsafePointerView get methods', () => {
  it('getInt8 should throw TypeError for invalid offset type', () => {
    const view = new UnsafePointerView(UnsafePointer.create(0n));
    assert.throws(() => {
      view.getInt8('invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.getBool', () => {
  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.getBool('invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.getCString', () => {
  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.getCString('invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.getPointer', () => {
  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.getPointer('invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.copyInto', () => {
  it('should throw TypeError when called without arguments', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.copyInto();
    }, {
      name: 'TypeError',
      message: 'First argument must be an ArrayBufferView',
    });
  });

  it('should throw TypeError when passed invalid destination', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.copyInto('not a view');
    }, {
      name: 'TypeError',
      message: 'First argument must be an ArrayBufferView',
    });
  });

  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    const dest = new Uint8Array(10);
    assert.throws(() => {
      view.copyInto(dest, 'invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.getArrayBuffer', () => {
  it('should throw TypeError when called without arguments', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.getArrayBuffer();
    }, {
      name: 'TypeError',
      message: 'byteLength argument must be a number',
    });
  });

  it('should throw TypeError when passed non-number byteLength', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.getArrayBuffer('not a number');
    }, {
      name: 'TypeError',
      message: 'byteLength argument must be a number',
    });
  });

  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.getArrayBuffer(10, 'invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.getCString()', () => {
  it('should throw TypeError when called without arguments', () => {
    assert.throws(() => {
      UnsafePointerView.getCString();
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed invalid pointer', () => {
    assert.throws(() => {
      UnsafePointerView.getCString('not a pointer');
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      UnsafePointerView.getCString(ptr, 'invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.copyInto()', () => {
  it('should throw TypeError when called without arguments', () => {
    assert.throws(() => {
      UnsafePointerView.copyInto();
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed invalid pointer', () => {
    assert.throws(() => {
      UnsafePointerView.copyInto('not a pointer', new Uint8Array(10));
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed invalid destination', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      UnsafePointerView.copyInto(ptr, 'not a view');
    }, {
      name: 'TypeError',
      message: 'The "destination" argument must be an ArrayBufferView.',
    });
  });

  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      UnsafePointerView.copyInto(ptr, new Uint8Array(10), 'invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView.getArrayBuffer (static)', () => {
  it('should throw TypeError when called without arguments', () => {
    assert.throws(() => {
      UnsafePointerView.getArrayBuffer();
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed invalid pointer', () => {
    assert.throws(() => {
      UnsafePointerView.getArrayBuffer('not a pointer', 10);
    }, {
      name: 'TypeError',
      message: 'The "pointer" argument must be a pointer object.',
    });
  });

  it('should throw TypeError when passed non-number byteLength', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      UnsafePointerView.getArrayBuffer(ptr, 'not a number');
    }, {
      name: 'TypeError',
      message: 'The "byteLength" argument must be a number.',
    });
  });

  it('should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      UnsafePointerView.getArrayBuffer(ptr, 10, 'invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });
});

describe('UnsafePointerView set methods', () => {
  it('setInt8 should throw TypeError when called without arguments', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setInt8();
    }, {
      name: 'TypeError',
      message: 'Expected a value argument',
    });
  });

  it('setInt8 should throw TypeError when called with only value', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setInt8(42);
    }, {
      name: 'TypeError',
      message: 'Expected value and optional offset arguments',
    });
  });

  it('setInt8 should throw TypeError for invalid offset type', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setInt8(42, 'invalid');
    }, {
      name: 'TypeError',
      message: 'Offset must be a number or BigInt',
    });
  });

  it('setUint8 should throw TypeError when called without arguments', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setUint8();
    }, {
      name: 'TypeError',
      message: 'Expected a value argument',
    });
  });

  it('setBigInt64 should throw TypeError when called without arguments', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setBigInt64();
    }, {
      name: 'TypeError',
      message: 'Expected a value argument',
    });
  });

  it('setBigInt64 should throw TypeError for non-BigInt value', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setBigInt64('not a bigint', 0);
    }, {
      name: 'TypeError',
      message: 'Invalid BigInt value',
    });
  });

  it('setBigUint64 should throw TypeError when called without arguments', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setBigUint64();
    }, {
      name: 'TypeError',
      message: 'Expected a value argument',
    });
  });

  it('setBigUint64 should throw TypeError for non-BigInt value', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const view = new UnsafePointerView(ptr);
    assert.throws(() => {
      view.setBigUint64('not a bigint', 0);
    }, {
      name: 'TypeError',
      message: 'Invalid BigInt value',
    });
  });
});
