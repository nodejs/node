'use strict';

require('../common');
const assert = require('node:assert');
const { describe, test } = require('node:test');

describe('Buffer.copy', () => {
  describe('Buffer to Buffer', () => {
    test('should copy entire source buffer to target buffer', () => {
      const src = Buffer.from([1, 2, 3, 4]);
      const dst = Buffer.alloc(4);

      Buffer.copy(src, dst, 0);

      assert.deepStrictEqual(dst, Buffer.from([1, 2, 3, 4]));
    });

    test('should copy source buffer to target buffer at offset', () => {
      const src = Buffer.from([1, 2, 3]);
      const dst = Buffer.from([0, 0, 0, 0, 0]);

      Buffer.copy(src, dst, 2);

      assert.deepStrictEqual(dst, Buffer.from([0, 0, 1, 2, 3]));
    });

    test('should return number of bytes copied', () => {
      const src = Buffer.from([1, 2, 3, 4]);
      const dst = Buffer.alloc(4);

      const bytesCopied = Buffer.copy(src, dst, 0);

      assert.strictEqual(bytesCopied, 4);
    });
  });

  describe('ArrayBuffer to Buffer', () => {
    test('should copy ArrayBuffer to Buffer', () => {
      const src = new Uint8Array([5, 6, 7, 8]).buffer;
      const dst = Buffer.alloc(4);

      Buffer.copy(src, dst, 0);

      assert.deepStrictEqual(dst, Buffer.from([5, 6, 7, 8]));
    });

    test('should copy ArrayBuffer to Buffer at offset', () => {
      const src = new Uint8Array([5, 6]).buffer;
      const dst = Buffer.from([0, 0, 0, 0]);

      Buffer.copy(src, dst, 1);

      assert.deepStrictEqual(dst, Buffer.from([0, 5, 6, 0]));
    });

    test('should handle overlapping memory in the same ArrayBuffer', () => {
      const ab = new Uint8Array([1, 2, 3, 4, 5, 0, 0]).buffer;
      const src = new Uint8Array(ab, 0, 5);
      const dst = new Uint8Array(ab, 2, 5);

      Buffer.copy(src, dst, 0);
      assert.deepStrictEqual(new Uint8Array(ab), new Uint8Array([1, 2, 1, 2, 3, 4, 5]));
    });

    test("should handle detached instances", () => {
      const src = new Uint8Array([5, 6]).buffer;
      src.transfer()
      const dst = Buffer.from([0, 0, 0, 0]);

      Buffer.copy(src, dst, 1);

      assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0]));
    })
  });

  describe('DataView to Buffer', () => {
    test('should copy DataView to Buffer', () => {
      const ab = new Uint8Array([9, 10, 11]).buffer;
      const src = new DataView(ab);
      const dst = Buffer.alloc(3);

      Buffer.copy(src, dst, 0);

      assert.deepStrictEqual(dst, Buffer.from([9, 10, 11]));
    });

    test('should copy DataView to Buffer at offset', () => {
      const ab = new Uint8Array([9, 10]).buffer;
      const src = new DataView(ab);
      const dst = Buffer.from([0, 0, 0, 0]);

      Buffer.copy(src, dst, 2);

      assert.deepStrictEqual(dst, Buffer.from([0, 0, 9, 10]));
    });
  });

  describe('Buffer to ArrayBuffer', () => {
    test('should copy Buffer to ArrayBuffer', () => {
      const src = Buffer.from([1, 2, 3]);
      const dst = new Uint8Array(3).buffer;

      Buffer.copy(src, dst, 0);

      assert.deepStrictEqual(new Uint8Array(dst), new Uint8Array([1, 2, 3]));
    });

    test('should copy Buffer to ArrayBuffer at offset', () => {
      const src = Buffer.from([1, 2]);
      const dst = new Uint8Array([0, 0, 0, 0]).buffer;

      Buffer.copy(src, dst, 1);

      assert.deepStrictEqual(new Uint8Array(dst), new Uint8Array([0, 1, 2, 0]));
    });
  });

  describe('Buffer to DataView', () => {
    test('should copy Buffer to DataView', () => {
      const src = Buffer.from([1, 2, 3]);
      const ab = new Uint8Array(3).buffer;
      const dst = new DataView(ab);

      Buffer.copy(src, dst, 0);

      assert.deepStrictEqual(new Uint8Array(ab), new Uint8Array([1, 2, 3]));
    });

    test('should copy Buffer to DataView at offset', () => {
      const src = Buffer.from([1, 2]);
      const ab = new Uint8Array([0, 0, 0, 0]).buffer;
      const dst = new DataView(ab);

      Buffer.copy(src, dst, 2);

      assert.deepStrictEqual(new Uint8Array(ab), new Uint8Array([0, 0, 1, 2]));
    });
  });

  describe('Partial copies', () => {
    test('should copy only available space in target', () => {
      const src = Buffer.from([1, 2, 3, 4, 5]);
      const dst = Buffer.alloc(3);

      const bytesCopied = Buffer.copy(src, dst, 0);

      assert.strictEqual(bytesCopied, 3);
      assert.deepStrictEqual(dst, Buffer.from([1, 2, 3]));
    });

    test('should copy partial data when offset reduces available space', () => {
      const src = Buffer.from([1, 2, 3, 4]);
      const dst = Buffer.alloc(5);

      const bytesCopied = Buffer.copy(src, dst, 3);

      assert.strictEqual(bytesCopied, 2);
      assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 1, 2]));
    });
  });

  describe('sourceStart and sourceEnd parameters', () => {
    describe('sourceStart parameter', () => {
      test('should copy from sourceStart position', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 2);

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([3, 4, 5, 0]));
      });

      test('should copy from sourceStart with target offset', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.from([0, 0, 0, 0, 0]);

        const bytesCopied = Buffer.copy(src, dst, 1, 2);

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([0, 3, 4, 5, 0]));
      });

      test('should handle sourceStart at end of source', () => {
        const src = Buffer.from([1, 2, 3, 4]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 4);

        assert.strictEqual(bytesCopied, 0);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0]));
      });

      test('should throw RangeError for sourceStart beyond source length', () => {
        const src = Buffer.from([1, 2, 3, 4]);
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(src, dst, 0, 10);
        }, {
          name: 'RangeError',
          message: /The value of "sourceStart" is out of range/
        });
      });
    });

    describe('sourceEnd parameter', () => {
      test('should copy up to sourceEnd position', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 0, 3);

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([1, 2, 3, 0]));
      });

      test('should copy partial range with sourceEnd', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.from([0, 0, 0, 0, 0]);

        const bytesCopied = Buffer.copy(src, dst, 1, 0, 2);

        assert.strictEqual(bytesCopied, 2);
        assert.deepStrictEqual(dst, Buffer.from([0, 1, 2, 0, 0]));
      });

      test('should clamp sourceEnd beyond source length', () => {
        const src = Buffer.from([1, 2, 3, 4]);
        const dst = Buffer.alloc(6);

        const bytesCopied = Buffer.copy(src, dst, 0, 0, 10);

        assert.strictEqual(bytesCopied, 4);
        assert.deepStrictEqual(dst, Buffer.from([1, 2, 3, 4, 0, 0]));
      });
    });

    describe('sourceStart and sourceEnd together', () => {
      test('should copy range from sourceStart to sourceEnd', () => {
        const src = Buffer.from([1, 2, 3, 4, 5, 6]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 2, 5);

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([3, 4, 5, 0]));
      });

      test('should copy range with target offset', () => {
        const src = Buffer.from([1, 2, 3, 4, 5, 6]);
        const dst = Buffer.from([0, 0, 0, 0, 0, 0]);

        const bytesCopied = Buffer.copy(src, dst, 2, 1, 4);

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 2, 3, 4, 0]));
      });

      test('should handle sourceStart >= sourceEnd (copy zero bytes)', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 3, 3);

        assert.strictEqual(bytesCopied, 0);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0]));
      });

      test('should handle sourceStart > sourceEnd (copy zero bytes)', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 4, 2);

        assert.strictEqual(bytesCopied, 0);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0]));
      });
    });

    describe('sourceStart/sourceEnd validation', () => {
      test('should throw RangeError for negative sourceStart', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(src, dst, 0, -1);
        }, {
          name: 'RangeError',
          message: /The value of "sourceStart" is out of range/
        });
      });

      test('should throw RangeError for negative sourceEnd', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(src, dst, 0, 0, -1);
        }, {
          name: 'RangeError',
          message: /The value of "sourceEnd" is out of range/
        });
      });

      test('should coerce string sourceStart to number', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, '2');

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([3, 4, 5, 0]));
      });

      test('should coerce string sourceEnd to number', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, 0, '3');

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([1, 2, 3, 0]));
      });

      test('should treat NaN sourceStart as 0', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 0, NaN);

        assert.strictEqual(bytesCopied, 4);
        assert.deepStrictEqual(dst, Buffer.from([1, 2, 3, 4]));
      });

      test('should treat NaN sourceEnd as 0', () => {
        const src = Buffer.from([1, 2, 3, 4, 5]);
        const dst = Buffer.alloc(6);

        const bytesCopied = Buffer.copy(src, dst, 0, 0, NaN);

        assert.strictEqual(bytesCopied, 0);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0, 0, 0]));
      });
    });
  });

  describe('Input validation', () => {
    describe('Null and undefined arguments', () => {
      test('should throw TypeError for null source', () => {
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(null, dst, 0);
        }, {
          name: 'TypeError',
          message: /The "source" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received null/
        });
      });

      test('should throw TypeError for undefined source', () => {
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(undefined, dst, 0);
        }, {
          name: 'TypeError',
          message: /The "source" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received undefined/
        });
      });

      test('should throw TypeError for null target', () => {
        const src = Buffer.from([1, 2, 3]);

        assert.throws(() => {
          Buffer.copy(src, null, 0);
        }, {
          name: 'TypeError',
          message: /The "target" argument must be an instance of Buffer, ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received null/
        });
      });

      test('should throw TypeError for undefined target', () => {
        const src = Buffer.from([1, 2, 3]);

        assert.throws(() => {
          Buffer.copy(src, undefined, 0);
        }, {
          name: 'TypeError',
          message: /The "target" argument must be an instance of Buffer, ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received undefined/
        });
      });
    });

    describe('Invalid type arguments', () => {
      test('should throw TypeError for string source', () => {
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy('hello', dst, 0);
        }, {
          name: 'TypeError',
          message: /The "source" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received type string \('hello'\)/
        });
      });

      test('should throw TypeError for number source', () => {
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(123, dst, 0);
        }, {
          name: 'TypeError',
          message: /The "source" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received type number \(123\)/
        });
      });

      test('should throw TypeError for plain object source', () => {
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy({ length: 4 }, dst, 0);
        }, {
          name: 'TypeError',
          message: /The "source" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received an instance of Object/
        });
      });

      test('should throw TypeError for array source', () => {
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy([1, 2, 3, 4], dst, 0);
        }, {
          name: 'TypeError',
          message: /The "source" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received an instance of Array/
        });
      });

      test('should throw TypeError for string target', () => {
        const src = Buffer.from([1, 2, 3]);

        assert.throws(() => {
          Buffer.copy(src, 'hello', 0);
        }, {
          name: 'TypeError',
          message: /The "target" argument must be an instance of Buffer, ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received type string \('hello'\)/
        });
      });

      test('should throw TypeError for number target', () => {
        const src = Buffer.from([1, 2, 3]);

        assert.throws(() => {
          Buffer.copy(src, 123, 0);
        }, {
          name: 'TypeError',
          message: /The "target" argument must be an instance of Buffer, ArrayBuffer, SharedArrayBuffer, or TypedArray\. Received type number \(123\)/
        });
      });
    });

    describe('Offset validation', () => {
      test('should throw RangeError for negative offset', () => {
        const src = Buffer.from([1, 2, 3]);
        const dst = Buffer.alloc(4);

        assert.throws(() => {
          Buffer.copy(src, dst, -1);
        }, {
          name: 'RangeError',
          message: /The value of "targetStart" is out of range\. It must be >= 0\. Received -1/
        });
      });

      test('should coerce non-number offset to number', () => {
        const src = Buffer.from([1, 2, 3]);
        const dst = Buffer.from([0, 0, 0, 0]);

        // String '2' should be coerced to number 2
        const bytesCopied = Buffer.copy(src, dst, '2');

        assert.strictEqual(bytesCopied, 2);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 1, 2]));
      });

      test('should treat NaN offset as 0', () => {
        const src = Buffer.from([1, 2, 3]);
        const dst = Buffer.from([0, 0, 0, 0]);

        // NaN should be coerced to 0
        const bytesCopied = Buffer.copy(src, dst, NaN);

        assert.strictEqual(bytesCopied, 3);
        assert.deepStrictEqual(dst, Buffer.from([1, 2, 3, 0]));
      });

      test('should return 0 for offset beyond target length', () => {
        const src = Buffer.from([1, 2, 3]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 5);

        assert.strictEqual(bytesCopied, 0);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0]));
      });

      test('should return 0 for offset equal to target length', () => {
        const src = Buffer.from([1, 2, 3]);
        const dst = Buffer.alloc(4);

        const bytesCopied = Buffer.copy(src, dst, 4);

        assert.strictEqual(bytesCopied, 0);
        assert.deepStrictEqual(dst, Buffer.from([0, 0, 0, 0]));
      });
    });
  });
});
